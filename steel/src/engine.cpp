#include <steel/engine.hpp>
#include "steel/fxaa_shaders.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace steel {

Engine::Engine(std::string_view title) {
    spdlog::info("steel::Engine initializing");

    uint32_t width = 1280;
    uint32_t height = 960;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    // Pick the largest predefined resolution that fits the primary display
    struct Resolution { uint32_t w, h; };
    constexpr std::array<Resolution, 9> candidates = {{
        {3200, 2400}, {2560, 1920}, {2048, 1536}, {1600, 1200}, {1440, 1080},
        {1280, 960}, {1024, 768}, {800, 600}, {640, 480},
    }};

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    SDL_Rect usable;
    if (primary && SDL_GetDisplayUsableBounds(primary, &usable)) {
        bool found = false;
        for (const auto& res : candidates) {
            if (static_cast<int>(res.w) < usable.w &&
                static_cast<int>(res.h) < usable.h) {
                width = res.w;
                height = res.h;
                found = true;
                break;
            }
        }
        if (!found) {
            width = candidates.back().w;
            height = candidates.back().h;
        }
        spdlog::info("Display usable area: {}x{}, selected window size: {}x{}",
                     usable.w, usable.h, width, height);
    } else {
        spdlog::warn("Could not query display, using fallback size: {}x{}", width, height);
    }

    window_ = SDL_CreateWindow(
        std::string(title).c_str(),
        static_cast<int>(width),
        static_cast<int>(height),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (!window_) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    create_instance(title);
    create_surface();
    pick_physical_device();
    depth_format_ = find_depth_format(physical_device_);
    create_device();
    create_allocator();
    create_swapchain();
    create_depth_resources();
    create_offscreen_target();
    create_render_pass();
    create_offscreen_framebuffer();
    create_fxaa_render_pass();
    create_fxaa_framebuffers();
    create_command_pool();
    create_sync_objects();
    create_fxaa_descriptors();
    create_fxaa_pipeline();
    create_imgui_render_pass();
    create_imgui_framebuffers();
    init_imgui();

    last_frame_time_ = SDL_GetTicks();

    spdlog::info("steel::Engine initialized successfully");
}

Engine::~Engine() {
    wait_idle();
    shutdown_imgui();

    // Destroy VMA-managed images before allocator
    depth_image_ = VmaImage{};
    offscreen_image_ = VmaImage{};

    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
    spdlog::info("steel::Engine destroyed");
}

void Engine::wait_idle() {
    if (*device_) {
        device_.waitIdle();
    }
}

// ---------------------------------------------------------------------------
// Event polling
// ---------------------------------------------------------------------------

bool Engine::poll_events() {
    // Reset per-frame input accumulators
    mouse_dx_ = 0.0f;
    mouse_dy_ = 0.0f;

    // Compute delta time
    uint64_t now = SDL_GetTicks();
    delta_time_ = static_cast<float>(now - last_frame_time_) / 1000.0f;
    last_frame_time_ = now;

    // Clamp delta time to avoid huge jumps (e.g. after breakpoint)
    if (delta_time_ > 0.1f) {
        delta_time_ = 0.1f;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Let ImGui process events when mouse is not captured
        if (imgui_initialized_ && !mouse_captured_) {
            ImGui_ImplSDL3_ProcessEvent(&event);
        }

        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            framebuffer_resized_ = true;
        }

        // Toggle ImGui with F3
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F3 &&
            !event.key.repeat) {
            imgui_enabled_ = !imgui_enabled_;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            // Don't capture mouse if ImGui wants it
            if (!imgui_enabled_ || !ImGui::GetIO().WantCaptureMouse) {
                mouse_captured_ = true;
                mouse_capture_first_frame_ = true;
                SDL_SetWindowRelativeMouseMode(window_, true);
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
            mouse_captured_ = false;
            SDL_SetWindowRelativeMouseMode(window_, false);
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION && mouse_captured_ && !mouse_capture_first_frame_) {
            mouse_dx_ += event.motion.xrel;
            mouse_dy_ += event.motion.yrel;
        }
    }
    mouse_capture_first_frame_ = false;
    return true;
}

const bool* Engine::keyboard_state() const {
    return SDL_GetKeyboardState(nullptr);
}

// ---------------------------------------------------------------------------
// Frame rendering
// ---------------------------------------------------------------------------

const vk::raii::CommandBuffer* Engine::begin_frame() {
    // Wait for this frame's fence
    auto wait_result = device_.waitForFences(*in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
    (void)wait_result;

    // Acquire next swapchain image
    vk::Result result;
    uint32_t image_index;
    try {
        auto [r, i] = swapchain_.acquireNextImage(UINT64_MAX, *image_available_[current_frame_]);
        result = r;
        image_index = i;
    } catch (const vk::OutOfDateKHRError&) {
        framebuffer_resized_ = false;
        recreate_swapchain();
        return nullptr;
    }

    if (result == vk::Result::eSuboptimalKHR) {
        framebuffer_resized_ = true;
    }

    current_image_index_ = image_index;

    // Only reset fence after we know we'll submit work
    device_.resetFences(*in_flight_fences_[current_frame_]);

    // Begin command buffer
    auto& cmd = command_buffers_[current_frame_];
    cmd.reset();
    cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Begin scene render pass (renders to offscreen target)
    std::array<vk::ClearValue, 2> clear_values = {
        vk::ClearValue{vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}},
        vk::ClearValue{vk::ClearDepthStencilValue{.depth = 1.0f, .stencil = 0}},
    };

    vk::RenderPassBeginInfo render_pass_info{
        .renderPass = *render_pass_,
        .framebuffer = *offscreen_framebuffer_,
        .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = swapchain_extent_},
        .clearValueCount = static_cast<uint32_t>(clear_values.size()),
        .pClearValues = clear_values.data(),
    };

    cmd.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);

    // Set viewport and scissor
    vk::Viewport viewport{
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(swapchain_extent_.width),
        .height = static_cast<float>(swapchain_extent_.height),
        .minDepth = 0.0f, .maxDepth = 1.0f};
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{.offset = {0, 0}, .extent = swapchain_extent_};
    cmd.setScissor(0, scissor);

    return &cmd;
}

void Engine::end_frame() {
    auto& cmd = command_buffers_[current_frame_];

    // End scene render pass
    cmd.endRenderPass();

    // Begin FXAA render pass (reads offscreen, writes to swapchain)
    vk::RenderPassBeginInfo fxaa_rp_info{
        .renderPass = *fxaa_render_pass_,
        .framebuffer = *fxaa_framebuffers_[current_image_index_],
        .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = swapchain_extent_},
    };

    cmd.beginRenderPass(fxaa_rp_info, vk::SubpassContents::eInline);

    vk::Viewport viewport{
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(swapchain_extent_.width),
        .height = static_cast<float>(swapchain_extent_.height),
        .minDepth = 0.0f, .maxDepth = 1.0f};
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{.offset = {0, 0}, .extent = swapchain_extent_};
    cmd.setScissor(0, scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *fxaa_pipeline_);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *fxaa_pipeline_layout_, 0, *fxaa_descriptor_set_, {});
    cmd.draw(3, 1, 0, 0);

    cmd.endRenderPass();

    // ImGui render pass — always runs for layout transition to ePresentSrcKHR.
    // When ImGui is disabled, the draw data is empty so this is essentially free.
    if (imgui_initialized_) {
        vk::RenderPassBeginInfo imgui_rp_info{
            .renderPass = *imgui_render_pass_,
            .framebuffer = *imgui_framebuffers_[current_image_index_],
            .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = swapchain_extent_},
        };

        cmd.beginRenderPass(imgui_rp_info, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);
        cmd.endRenderPass();
    }

    cmd.end();

    // Submit
    vk::Semaphore          wait_semaphores[]   = {*image_available_[current_frame_]};
    vk::PipelineStageFlags wait_stages[]       = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore          signal_semaphores[] = {*render_finished_[current_image_index_]};

    vk::CommandBuffer cmd_handle = *cmd;
    vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_handle,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores,
    };

    graphics_queue_.submit(submit_info, *in_flight_fences_[current_frame_]);

    // Present
    vk::SwapchainKHR swapchain_handle = *swapchain_;
    vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &swapchain_handle,
        .pImageIndices = &current_image_index_,
    };

    bool needs_recreate = framebuffer_resized_;
    try {
        auto present_result = present_queue_.presentKHR(present_info);
        if (present_result == vk::Result::eSuboptimalKHR) {
            needs_recreate = true;
        }
    } catch (const vk::OutOfDateKHRError&) {
        needs_recreate = true;
    }

    if (needs_recreate) {
        framebuffer_resized_ = false;
        recreate_swapchain();
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ---------------------------------------------------------------------------
// Instance
// ---------------------------------------------------------------------------

void Engine::create_instance(std::string_view title) {
    std::string title_str{title};
    vk::ApplicationInfo app_info{
        .pApplicationName = title_str.c_str(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "steel",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    // Get required extensions from SDL
    uint32_t sdl_ext_count = 0;
    const char* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
    std::vector<const char*> extensions(sdl_exts, sdl_exts + sdl_ext_count);

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    vk::InstanceCreateFlags flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#else
    vk::InstanceCreateFlags flags{};
#endif

    // Validation layers in debug builds (only if available)
    std::vector<const char*> layers;
#ifndef NDEBUG
    {
        auto available = context_.enumerateInstanceLayerProperties();
        bool found = false;
        for (const auto& layer : available) {
            if (std::string_view(layer.layerName.data()) == "VK_LAYER_KHRONOS_validation") {
                found = true;
                break;
            }
        }
        if (found) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            spdlog::info("Vulkan validation layers enabled");
        } else {
            spdlog::warn("VK_LAYER_KHRONOS_validation not found — running without validation layers");
        }
    }
#endif

    vk::InstanceCreateInfo create_info{
        .flags = flags,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    instance_ = vk::raii::Instance{context_, create_info};
    spdlog::info("Vulkan instance created (API 1.3)");
}

// ---------------------------------------------------------------------------
// Surface
// ---------------------------------------------------------------------------

void Engine::create_surface() {
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window_, *instance_, nullptr, &raw_surface)) {
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    surface_ = vk::raii::SurfaceKHR{instance_, raw_surface};
}

// ---------------------------------------------------------------------------
// Physical device
// ---------------------------------------------------------------------------

bool Engine::is_device_suitable(const vk::raii::PhysicalDevice& dev) const {
    // Check for graphics and present queue families
    auto queue_families = dev.getQueueFamilyProperties();
    bool has_graphics = false;
    bool has_present  = false;

    for (uint32_t i = 0; i < queue_families.size(); ++i) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            has_graphics = true;
        }
        if (dev.getSurfaceSupportKHR(i, *surface_)) {
            has_present = true;
        }
        if (has_graphics && has_present) break;
    }

    if (!has_graphics || !has_present) return false;

    // Check for swapchain extension support
    auto extensions = dev.enumerateDeviceExtensionProperties();
    bool has_swapchain = false;
    for (const auto& ext : extensions) {
        if (std::string_view(ext.extensionName.data()) == VK_KHR_SWAPCHAIN_EXTENSION_NAME) {
            has_swapchain = true;
            break;
        }
    }
    if (!has_swapchain) return false;

    // Check for at least one surface format and present mode
    auto formats = dev.getSurfaceFormatsKHR(*surface_);
    auto modes   = dev.getSurfacePresentModesKHR(*surface_);
    if (formats.empty() || modes.empty()) return false;

    // Check for a supported depth format
    if (find_depth_format(dev) == vk::Format::eUndefined) return false;

    return true;
}

vk::Format Engine::find_depth_format(const vk::raii::PhysicalDevice& dev) const {
    constexpr std::array candidates = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
    };

    for (auto format : candidates) {
        auto props = dev.getFormatProperties(format);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    return vk::Format::eUndefined;
}

void Engine::pick_physical_device() {
    auto devices = vk::raii::PhysicalDevices{instance_};
    if (devices.empty()) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    // Filter to suitable devices, then prefer discrete GPU
    std::vector<std::pair<size_t, vk::PhysicalDeviceType>> candidates;
    for (size_t i = 0; i < devices.size(); ++i) {
        if (!is_device_suitable(devices[i])) {
            auto props = devices[i].getProperties();
            spdlog::debug("Skipping unsuitable GPU: {}", props.deviceName.data());
            continue;
        }
        candidates.emplace_back(i, devices[i].getProperties().deviceType);
    }

    if (candidates.empty()) {
        throw std::runtime_error("No suitable Vulkan GPU found (need graphics+present queues, "
                                 "swapchain support, surface formats, and a depth format)");
    }

    // Pick best: discrete > integrated > virtual > other
    auto score = [](vk::PhysicalDeviceType type) -> int {
        switch (type) {
            case vk::PhysicalDeviceType::eDiscreteGpu:   return 4;
            case vk::PhysicalDeviceType::eIntegratedGpu: return 3;
            case vk::PhysicalDeviceType::eVirtualGpu:    return 2;
            default:                                     return 1;
        }
    };

    auto best = std::ranges::max_element(candidates,
        [&](const auto& a, const auto& b) { return score(a.second) < score(b.second); });

    physical_device_ = std::move(devices[best->first]);
    auto props = physical_device_.getProperties();
    spdlog::info("Selected GPU: {} ({})", props.deviceName.data(), vk::to_string(props.deviceType));
}

// ---------------------------------------------------------------------------
// Logical device
// ---------------------------------------------------------------------------

void Engine::create_device() {
    // Find queue families
    auto queue_families = physical_device_.getQueueFamilyProperties();
    bool found_graphics = false;
    bool found_present  = false;

    for (uint32_t i = 0; i < queue_families.size(); ++i) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            graphics_family_index_ = i;
            found_graphics = true;
        }
        if (physical_device_.getSurfaceSupportKHR(i, *surface_)) {
            present_family_index_ = i;
            found_present = true;
        }
        if (found_graphics && found_present) break;
    }

    if (!found_graphics || !found_present) {
        throw std::runtime_error("Could not find suitable queue families");
    }

    float queue_priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.push_back({.queueFamilyIndex = graphics_family_index_, .queueCount = 1, .pQueuePriorities = &queue_priority});
    if (present_family_index_ != graphics_family_index_) {
        queue_create_infos.push_back({.queueFamilyIndex = present_family_index_, .queueCount = 1, .pQueuePriorities = &queue_priority});
    }

    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
        "VK_KHR_portability_subset",
#endif
    };

    vk::PhysicalDeviceFeatures features{};

    vk::DeviceCreateInfo create_info{
        .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
        .pEnabledFeatures = &features,
    };

    device_ = vk::raii::Device{physical_device_, create_info};
    graphics_queue_ = device_.getQueue(graphics_family_index_, 0);
    present_queue_  = device_.getQueue(present_family_index_, 0);

    spdlog::info("Logical device created (graphics queue family: {}, present queue family: {})",
                 graphics_family_index_, present_family_index_);
}

// ---------------------------------------------------------------------------
// Swapchain
// ---------------------------------------------------------------------------

void Engine::create_swapchain() {
    auto capabilities = physical_device_.getSurfaceCapabilitiesKHR(*surface_);
    auto formats      = physical_device_.getSurfaceFormatsKHR(*surface_);

    // Pick surface format: prefer B8G8R8A8_SRGB
    surface_format_ = formats[0];
    for (const auto& fmt : formats) {
        if (fmt.format == vk::Format::eB8G8R8A8Srgb &&
            fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surface_format_ = fmt;
            break;
        }
    }

    // Extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        swapchain_extent_ = capabilities.currentExtent;
    } else {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        swapchain_extent_.width  = std::clamp(static_cast<uint32_t>(w),
            capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
        swapchain_extent_.height = std::clamp(static_cast<uint32_t>(h),
            capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    vk::SwapchainKHR old_swapchain = *swapchain_;

    vk::SwapchainCreateInfoKHR create_info{
        .surface = *surface_,
        .minImageCount = image_count,
        .imageFormat = surface_format_.format,
        .imageColorSpace = surface_format_.colorSpace,
        .imageExtent = swapchain_extent_,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    if (graphics_family_index_ != present_family_index_) {
        std::array<uint32_t, 2> indices = {graphics_family_index_, present_family_index_};
        create_info.imageSharingMode      = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = static_cast<uint32_t>(indices.size());
        create_info.pQueueFamilyIndices   = indices.data();
    }

    swapchain_ = vk::raii::SwapchainKHR{device_, create_info};
    swapchain_images_ = swapchain_.getImages();

    // Create image views
    swapchain_image_views_.clear();
    for (auto image : swapchain_images_) {
        vk::ImageViewCreateInfo view_info{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = surface_format_.format,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
        };
        swapchain_image_views_.emplace_back(device_, view_info);
    }

    spdlog::info("Swapchain created ({}x{}, {} images, format {})",
                 swapchain_extent_.width, swapchain_extent_.height,
                 swapchain_images_.size(), vk::to_string(surface_format_.format));
}

// ---------------------------------------------------------------------------
// Depth buffer
// ---------------------------------------------------------------------------

void Engine::create_depth_resources() {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = static_cast<VkFormat>(depth_format_);
    image_info.extent = {swapchain_extent_.width, swapchain_extent_.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    depth_image_ = VmaImage{};
    depth_image_.allocator = allocator_;
    VkResult result = vmaCreateImage(allocator_, &image_info, &alloc_info,
                                     &depth_image_.image, &depth_image_.allocation, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage failed for depth buffer");
    }

    vk::ImageViewCreateInfo view_info{
        .image = vk::Image{depth_image_.image},
        .viewType = vk::ImageViewType::e2D,
        .format = depth_format_,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
    };

    depth_image_view_ = vk::raii::ImageView{device_, view_info};
    spdlog::info("Depth buffer created (format {})", vk::to_string(depth_format_));
}

// ---------------------------------------------------------------------------
// Offscreen render target
// ---------------------------------------------------------------------------

void Engine::create_offscreen_target() {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = static_cast<VkFormat>(surface_format_.format);
    image_info.extent = {swapchain_extent_.width, swapchain_extent_.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    offscreen_image_ = VmaImage{};
    offscreen_image_.allocator = allocator_;
    VkResult result = vmaCreateImage(allocator_, &image_info, &alloc_info,
                                     &offscreen_image_.image, &offscreen_image_.allocation, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage failed for offscreen target");
    }

    vk::ImageViewCreateInfo view_info{
        .image = vk::Image{offscreen_image_.image},
        .viewType = vk::ImageViewType::e2D,
        .format = surface_format_.format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
    };

    offscreen_image_view_ = vk::raii::ImageView{device_, view_info};
    spdlog::info("Offscreen render target created ({}x{})", swapchain_extent_.width, swapchain_extent_.height);
}

// ---------------------------------------------------------------------------
// Render pass (scene — now outputs to shader-read-only for FXAA)
// ---------------------------------------------------------------------------

void Engine::create_render_pass() {
    vk::AttachmentDescription color_attachment{
        .format = surface_format_.format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::AttachmentDescription depth_attachment{
        .format = depth_format_,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    vk::AttachmentReference color_ref{.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};
    vk::AttachmentReference depth_ref{.attachment = 1, .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
    };

    std::array<vk::AttachmentDescription, 2> attachments = {color_attachment, depth_attachment};

    vk::RenderPassCreateInfo create_info{
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    render_pass_ = vk::raii::RenderPass{device_, create_info};
}

// ---------------------------------------------------------------------------
// Offscreen framebuffer (scene pass)
// ---------------------------------------------------------------------------

void Engine::create_offscreen_framebuffer() {
    std::array<vk::ImageView, 2> attachments = {*offscreen_image_view_, *depth_image_view_};

    vk::FramebufferCreateInfo create_info{
        .renderPass = *render_pass_,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = swapchain_extent_.width,
        .height = swapchain_extent_.height,
        .layers = 1,
    };

    offscreen_framebuffer_ = vk::raii::Framebuffer{device_, create_info};
}

// ---------------------------------------------------------------------------
// FXAA render pass
// ---------------------------------------------------------------------------

void Engine::create_fxaa_render_pass() {
    vk::AttachmentDescription color_attachment{
        .format = surface_format_.format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eDontCare,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference color_ref{.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
    };

    vk::RenderPassCreateInfo create_info{
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    fxaa_render_pass_ = vk::raii::RenderPass{device_, create_info};
    spdlog::info("FXAA render pass created");
}

// ---------------------------------------------------------------------------
// FXAA framebuffers (per swapchain image)
// ---------------------------------------------------------------------------

void Engine::create_fxaa_framebuffers() {
    fxaa_framebuffers_.clear();
    for (const auto& image_view : swapchain_image_views_) {
        vk::ImageView attachment = *image_view;

        vk::FramebufferCreateInfo create_info{
            .renderPass = *fxaa_render_pass_,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .width = swapchain_extent_.width,
            .height = swapchain_extent_.height,
            .layers = 1,
        };

        fxaa_framebuffers_.emplace_back(device_, create_info);
    }
}

// ---------------------------------------------------------------------------
// FXAA descriptors
// ---------------------------------------------------------------------------

void Engine::create_fxaa_descriptors() {
    // Sampler
    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    fxaa_sampler_ = vk::raii::Sampler{device_, sampler_info};

    // Descriptor set layout
    vk::DescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
    };

    vk::DescriptorSetLayoutCreateInfo layout_info{
        .bindingCount = 1,
        .pBindings = &binding,
    };
    fxaa_descriptor_set_layout_ = vk::raii::DescriptorSetLayout{device_, layout_info};

    // Pipeline layout
    vk::PipelineLayoutCreateInfo pl_layout_info{
        .setLayoutCount = 1,
        .pSetLayouts = &*fxaa_descriptor_set_layout_,
    };
    fxaa_pipeline_layout_ = vk::raii::PipelineLayout{device_, pl_layout_info};

    // Descriptor pool
    vk::DescriptorPoolSize pool_size{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
    };

    vk::DescriptorPoolCreateInfo pool_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    fxaa_descriptor_pool_ = vk::raii::DescriptorPool{device_, pool_info};

    // Allocate descriptor set
    vk::DescriptorSetAllocateInfo alloc_info{
        .descriptorPool = *fxaa_descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*fxaa_descriptor_set_layout_,
    };
    auto sets = device_.allocateDescriptorSets(alloc_info);
    fxaa_descriptor_set_ = std::move(sets[0]);

    // Write initial descriptor
    update_fxaa_descriptor();

    spdlog::info("FXAA descriptors created");
}

void Engine::update_fxaa_descriptor() {
    vk::DescriptorImageInfo image_info{
        .sampler = *fxaa_sampler_,
        .imageView = *offscreen_image_view_,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::WriteDescriptorSet write{
        .dstSet = *fxaa_descriptor_set_,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &image_info,
    };

    device_.updateDescriptorSets(write, {});
}

// ---------------------------------------------------------------------------
// FXAA pipeline
// ---------------------------------------------------------------------------

void Engine::create_fxaa_pipeline() {
    // Create shader modules from embedded SPIR-V
    vk::ShaderModuleCreateInfo vert_info{
        .codeSize = shaders::fullscreen_vert.size() * sizeof(uint32_t),
        .pCode = shaders::fullscreen_vert.data(),
    };
    vk::raii::ShaderModule vert_module{device_, vert_info};

    vk::ShaderModuleCreateInfo frag_info{
        .codeSize = shaders::fxaa_frag.size() * sizeof(uint32_t),
        .pCode = shaders::fxaa_frag.data(),
    };
    vk::raii::ShaderModule frag_module{device_, frag_info};

    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {{
        {.stage = vk::ShaderStageFlagBits::eVertex,   .module = *vert_module, .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = *frag_module, .pName = "main"},
    }};

    // Empty vertex input (fullscreen triangle generated in vertex shader)
    vk::PipelineVertexInputStateCreateInfo vertex_input{};

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Dynamic viewport and scissor
    std::array<vk::DynamicState, 2> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state{
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = VK_FALSE,
    };

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = vk::BlendFactor::eOne,
        .dstColorBlendFactor = vk::BlendFactor::eZero,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo color_blending{
        .logicOpEnable = VK_FALSE,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = {{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = *fxaa_pipeline_layout_,
        .renderPass = *fxaa_render_pass_,
        .subpass = 0,
    };

    fxaa_pipeline_ = vk::raii::Pipeline{device_, nullptr, pipeline_info};
    spdlog::info("FXAA pipeline created");
}

// ---------------------------------------------------------------------------
// Command pool & buffers
// ---------------------------------------------------------------------------

void Engine::create_command_pool() {
    vk::CommandPoolCreateInfo pool_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = graphics_family_index_,
    };
    command_pool_ = vk::raii::CommandPool{device_, pool_info};

    vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *command_pool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };
    command_buffers_ = device_.allocateCommandBuffers(alloc_info);
}

// ---------------------------------------------------------------------------
// Sync objects
// ---------------------------------------------------------------------------

void Engine::create_sync_objects() {
    auto image_count = static_cast<uint32_t>(swapchain_images_.size());

    image_available_.reserve(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences_.reserve(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        image_available_.emplace_back(device_, vk::SemaphoreCreateInfo{});
        in_flight_fences_.emplace_back(device_, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    }

    // render_finished semaphores are per-swapchain-image (indexed by acquired image)
    // because the presentation engine may hold them until the image is re-acquired
    render_finished_.reserve(image_count);
    for (uint32_t i = 0; i < image_count; ++i) {
        render_finished_.emplace_back(device_, vk::SemaphoreCreateInfo{});
    }

    spdlog::info("Created {} frames-in-flight sync sets, {} present semaphores",
                 MAX_FRAMES_IN_FLIGHT, image_count);
}

// ---------------------------------------------------------------------------
// ImGui
// ---------------------------------------------------------------------------

void Engine::create_imgui_render_pass() {
    vk::AttachmentDescription color_attachment{
        .format = surface_format_.format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentReference color_ref{.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
    };

    vk::RenderPassCreateInfo create_info{
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    imgui_render_pass_ = vk::raii::RenderPass{device_, create_info};
    spdlog::info("ImGui render pass created");
}

void Engine::create_imgui_framebuffers() {
    imgui_framebuffers_.clear();
    for (const auto& image_view : swapchain_image_views_) {
        vk::ImageView attachment = *image_view;

        vk::FramebufferCreateInfo create_info{
            .renderPass = *imgui_render_pass_,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .width = swapchain_extent_.width,
            .height = swapchain_extent_.height,
            .layers = 1,
        };

        imgui_framebuffers_.emplace_back(device_, create_info);
    }
}

void Engine::init_imgui() {
    // Create a dedicated descriptor pool for ImGui
    std::array<vk::DescriptorPoolSize, 1> pool_sizes = {{
        {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1},
    }};

    vk::DescriptorPoolCreateInfo pool_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    imgui_descriptor_pool_ = vk::raii::DescriptorPool{device_, pool_info};

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Initialize SDL3 backend
    ImGui_ImplSDL3_InitForVulkan(window_);

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = *instance_;
    init_info.PhysicalDevice = *physical_device_;
    init_info.Device = *device_;
    init_info.QueueFamily = graphics_family_index_;
    init_info.Queue = *graphics_queue_;
    init_info.DescriptorPool = *imgui_descriptor_pool_;
    init_info.PipelineInfoMain.RenderPass = *imgui_render_pass_;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.ImageCount = static_cast<uint32_t>(swapchain_images_.size());

    ImGui_ImplVulkan_Init(&init_info);

    imgui_initialized_ = true;
    spdlog::info("ImGui initialized");
}

void Engine::shutdown_imgui() {
    if (!imgui_initialized_) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    imgui_initialized_ = false;
}

void Engine::imgui_begin() {
    if (!imgui_initialized_) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Engine::imgui_end() {
    if (!imgui_initialized_) return;

    ImGui::Render();
}

// ---------------------------------------------------------------------------
// Swapchain recreation (window resize)
// ---------------------------------------------------------------------------

void Engine::recreate_swapchain() {
    // Handle minimized window — wait until it has a nonzero size
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    while (w == 0 || h == 0) {
        SDL_WaitEvent(nullptr);
        SDL_GetWindowSizeInPixels(window_, &w, &h);
    }

    device_.waitIdle();

    // Destroy extent-dependent resources in reverse creation order
    imgui_framebuffers_.clear();
    fxaa_framebuffers_.clear();
    offscreen_framebuffer_ = vk::raii::Framebuffer{nullptr};
    depth_image_view_ = vk::raii::ImageView{nullptr};
    depth_image_ = VmaImage{};
    offscreen_image_view_ = vk::raii::ImageView{nullptr};
    offscreen_image_ = VmaImage{};
    swapchain_image_views_.clear();

    create_swapchain();
    create_depth_resources();
    create_offscreen_target();
    create_offscreen_framebuffer();
    create_fxaa_framebuffers();
    create_imgui_framebuffers();
    update_fxaa_descriptor();
    current_frame_ = 0;

    spdlog::info("Swapchain recreated ({}x{})", swapchain_extent_.width, swapchain_extent_.height);
}

// ---------------------------------------------------------------------------
// VMA allocator
// ---------------------------------------------------------------------------

void Engine::create_allocator() {
    VmaVulkanFunctions vma_funcs{};
    vma_funcs.vkGetInstanceProcAddr = context_.getDispatcher()->vkGetInstanceProcAddr;
    vma_funcs.vkGetDeviceProcAddr = instance_.getDispatcher()->vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo create_info{};
    create_info.instance = *instance_;
    create_info.physicalDevice = *physical_device_;
    create_info.device = *device_;
    create_info.pVulkanFunctions = &vma_funcs;
    create_info.vulkanApiVersion = VK_API_VERSION_1_3;

    VkResult result = vmaCreateAllocator(&create_info, &allocator_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateAllocator failed");
    }
    spdlog::info("VMA allocator created");
}

} // namespace steel

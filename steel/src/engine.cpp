#include <steel/engine.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace steel {

// Used by is_device_suitable() before Swapchain exists.
static vk::Format find_supported_depth_format(const vk::raii::PhysicalDevice& dev) {
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
    create_device();
    create_allocator();

    swapchain_.create(physical_device_, device_, surface_, allocator_, window_,
                      graphics_family_index_, present_family_index_);

    create_command_pool();
    create_sync_objects();

    fxaa_pass_.create(device_, swapchain_.color_format(), swapchain_.extent(),
                      swapchain_.image_views(), swapchain_.offscreen_image_view());
    imgui_pass_.create(instance_, physical_device_, device_,
                       graphics_family_index_, graphics_queue_,
                       swapchain_.color_format(), swapchain_.extent(),
                       swapchain_.image_views(),
                       static_cast<uint32_t>(swapchain_.images().size()),
                       window_);

    last_frame_time_ = SDL_GetTicks();

    spdlog::info("steel::Engine initialized successfully");
}

Engine::~Engine() {
    wait_idle();
    imgui_pass_.shutdown();

    // Flush all deferred resources before destroying GPU objects
    deferred_.clear();

    // Destroy swapchain resources (VmaImages) before the allocator
    swapchain_ = Swapchain{};

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

void Engine::flush_deferred() {
    while (!deferred_.empty() && deferred_.front().frames_remaining == 0) {
        deferred_.pop_front();
    }
    for (auto& entry : deferred_) {
        --entry.frames_remaining;
    }
}

// ---------------------------------------------------------------------------
// Event polling
// ---------------------------------------------------------------------------

bool Engine::poll_events() {
    // Compute delta time
    uint64_t now = SDL_GetTicks();
    delta_time_ = static_cast<float>(now - last_frame_time_) / 1000.0f;
    last_frame_time_ = now;

    // Clamp delta time to avoid huge jumps (e.g. after breakpoint)
    if (delta_time_ > 0.1f) {
        delta_time_ = 0.1f;
    }

    bool should_quit = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            should_quit = true;
            continue;
        }
        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            framebuffer_resized_ = true;
        }

        if (event_callback_) {
            event_callback_(event);
        }
    }
    return !should_quit;
}

// ---------------------------------------------------------------------------
// Frame rendering
// ---------------------------------------------------------------------------

const vk::raii::CommandBuffer* Engine::begin_frame() {
    // Wait for this frame's fence
    auto wait_result = device_.waitForFences(*in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
    (void)wait_result;

    // Flush deferred resource destructions now that the fence has signaled
    flush_deferred();

    // Acquire next swapchain image
    vk::Result result;
    uint32_t image_index;
    try {
        auto [r, i] = swapchain_.handle().acquireNextImage(UINT64_MAX, *image_available_[current_frame_]);
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
        vk::ClearValue{vk::ClearColorValue{std::array<float, 4>{0.53f, 0.71f, 0.92f, 1.0f}}},
        vk::ClearValue{vk::ClearDepthStencilValue{.depth = 1.0f, .stencil = 0}},
    };

    auto ext = swapchain_.extent();

    vk::RenderPassBeginInfo render_pass_info{
        .renderPass = *swapchain_.render_pass(),
        .framebuffer = *swapchain_.offscreen_framebuffer(),
        .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = ext},
        .clearValueCount = static_cast<uint32_t>(clear_values.size()),
        .pClearValues = clear_values.data(),
    };

    cmd.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);

    // Set viewport and scissor
    vk::Viewport viewport{
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(ext.width),
        .height = static_cast<float>(ext.height),
        .minDepth = 0.0f, .maxDepth = 1.0f};
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{.offset = {0, 0}, .extent = ext};
    cmd.setScissor(0, scissor);

    return &cmd;
}

void Engine::end_frame() {
    auto& cmd = command_buffers_[current_frame_];
    auto ext = swapchain_.extent();

    // End scene render pass
    cmd.endRenderPass();

    // FXAA post-process pass
    fxaa_pass_.apply(cmd, current_image_index_, ext);

    // ImGui render pass — always runs for layout transition to ePresentSrcKHR.
    // When ImGui is disabled, the draw data is empty so this is essentially free.
    imgui_pass_.render(cmd, current_image_index_, ext);

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
    vk::SwapchainKHR swapchain_handle = *swapchain_.handle();
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
    if (find_supported_depth_format(dev) == vk::Format::eUndefined) return false;

    return true;
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
    auto image_count = static_cast<uint32_t>(swapchain_.images().size());

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

    swapchain_.recreate(physical_device_, device_, surface_, allocator_, window_,
                        graphics_family_index_, present_family_index_);

    fxaa_pass_.recreate(device_, swapchain_.extent(),
                        swapchain_.image_views(), swapchain_.offscreen_image_view());
    imgui_pass_.recreate(device_, swapchain_.extent(), swapchain_.image_views());
    current_frame_ = 0;
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

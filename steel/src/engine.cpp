#include <steel/engine.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace steel {

Engine::Engine(std::string_view title, uint32_t width, uint32_t height) {
    spdlog::info("steel::Engine initializing ({}x{})", width, height);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    window_ = SDL_CreateWindow(
        std::string(title).c_str(),
        static_cast<int>(width),
        static_cast<int>(height),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!window_) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    create_instance(title);
    create_surface();
    pick_physical_device();
    create_device();
    create_swapchain();
    create_depth_resources();
    create_render_pass();
    create_framebuffers();
    create_command_pool();
    create_sync_objects();

    spdlog::info("steel::Engine initialized successfully");
}

Engine::~Engine() {
    wait_idle();
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
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Frame rendering
// ---------------------------------------------------------------------------

const vk::raii::CommandBuffer* Engine::begin_frame() {
    // Wait for this frame's fence
    auto wait_result = device_.waitForFences(*in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
    (void)wait_result;

    // Acquire next swapchain image
    auto [result, image_index] = swapchain_.acquireNextImage(UINT64_MAX, *image_available_[current_frame_]);
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        return nullptr;
    }
    current_image_index_ = image_index;

    // Only reset fence after we know we'll submit work
    device_.resetFences(*in_flight_fences_[current_frame_]);

    // Begin command buffer
    auto& cmd = command_buffers_[current_frame_];
    cmd.reset();
    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Begin render pass
    std::array<vk::ClearValue, 2> clear_values = {
        vk::ClearValue{vk::ClearColorValue{std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f}}},
        vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}},
    };

    vk::RenderPassBeginInfo render_pass_info{
        *render_pass_,
        *framebuffers_[current_image_index_],
        vk::Rect2D{{0, 0}, swapchain_extent_},
        clear_values,
    };

    cmd.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);

    // Set viewport and scissor
    vk::Viewport viewport{
        0.0f, 0.0f,
        static_cast<float>(swapchain_extent_.width),
        static_cast<float>(swapchain_extent_.height),
        0.0f, 1.0f};
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{{0, 0}, swapchain_extent_};
    cmd.setScissor(0, scissor);

    return &cmd;
}

void Engine::end_frame() {
    auto& cmd = command_buffers_[current_frame_];
    cmd.endRenderPass();
    cmd.end();

    // Submit
    vk::Semaphore          wait_semaphores[]   = {*image_available_[current_frame_]};
    vk::PipelineStageFlags wait_stages[]       = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore          signal_semaphores[] = {*render_finished_[current_frame_]};

    vk::SubmitInfo submit_info{
        wait_semaphores,
        wait_stages,
        *cmd,
        signal_semaphores,
    };

    graphics_queue_.submit(submit_info, *in_flight_fences_[current_frame_]);

    // Present
    vk::PresentInfoKHR present_info{
        signal_semaphores,
        *swapchain_,
        current_image_index_,
    };

    auto present_result = present_queue_.presentKHR(present_info);
    (void)present_result;

    current_frame_ = (current_frame_ + 1) % static_cast<uint32_t>(swapchain_images_.size());
}

// ---------------------------------------------------------------------------
// Instance
// ---------------------------------------------------------------------------

void Engine::create_instance(std::string_view title) {
    vk::ApplicationInfo app_info{
        std::string(title).c_str(),
        VK_MAKE_VERSION(1, 0, 0),
        "steel",
        VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_3,
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

    // Validation layers in debug builds
    std::vector<const char*> layers;
#ifndef NDEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
    spdlog::info("Vulkan validation layers enabled");
#endif

    vk::InstanceCreateInfo create_info{
        flags,
        &app_info,
        layers,
        extensions,
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

void Engine::pick_physical_device() {
    auto devices = vk::raii::PhysicalDevices{instance_};
    if (devices.empty()) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    // Prefer discrete GPU, otherwise take the first one.
    for (auto& dev : devices) {
        auto props = dev.getProperties();
        if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            physical_device_ = std::move(dev);
            spdlog::info("Selected GPU: {} (discrete)", physical_device_.getProperties().deviceName.data());
            return;
        }
    }
    physical_device_ = std::move(devices[0]);
    spdlog::info("Selected GPU: {}", physical_device_.getProperties().deviceName.data());
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
    queue_create_infos.push_back({{}, graphics_family_index_, 1, &queue_priority});
    if (present_family_index_ != graphics_family_index_) {
        queue_create_infos.push_back({{}, present_family_index_, 1, &queue_priority});
    }

    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
        "VK_KHR_portability_subset",
#endif
    };

    vk::PhysicalDeviceFeatures features{};

    vk::DeviceCreateInfo create_info{
        {},
        queue_create_infos,
        {},
        device_extensions,
        &features,
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

    vk::SwapchainCreateInfoKHR create_info{
        {},
        *surface_,
        image_count,
        surface_format_.format,
        surface_format_.colorSpace,
        swapchain_extent_,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive,
        {},
        capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::PresentModeKHR::eFifo,
        VK_TRUE,
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
            {},
            image,
            vk::ImageViewType::e2D,
            surface_format_.format,
            {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
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
    vk::ImageCreateInfo image_info{
        {},
        vk::ImageType::e2D,
        depth_format_,
        {swapchain_extent_.width, swapchain_extent_.height, 1},
        1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::SharingMode::eExclusive,
        {},
        vk::ImageLayout::eUndefined,
    };

    depth_image_ = vk::raii::Image{device_, image_info};

    auto mem_requirements = depth_image_.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{
        mem_requirements.size,
        find_memory_type(mem_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal),
    };

    depth_memory_ = vk::raii::DeviceMemory{device_, alloc_info};
    depth_image_.bindMemory(*depth_memory_, 0);

    vk::ImageViewCreateInfo view_info{
        {},
        *depth_image_,
        vk::ImageViewType::e2D,
        depth_format_,
        {},
        {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1},
    };

    depth_image_view_ = vk::raii::ImageView{device_, view_info};
    spdlog::info("Depth buffer created (format {})", vk::to_string(depth_format_));
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------

void Engine::create_render_pass() {
    vk::AttachmentDescription color_attachment{
        {},
        surface_format_.format,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentDescription depth_attachment{
        {},
        depth_format_,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eDontCare,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    vk::AttachmentReference color_ref{0, vk::ImageLayout::eColorAttachmentOptimal};
    vk::AttachmentReference depth_ref{1, vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::SubpassDescription subpass{
        {},
        vk::PipelineBindPoint::eGraphics,
        {},
        color_ref,
        {},
        &depth_ref,
    };

    vk::SubpassDependency dependency{
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        {},
        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
    };

    std::array<vk::AttachmentDescription, 2> attachments = {color_attachment, depth_attachment};

    vk::RenderPassCreateInfo create_info{
        {},
        attachments,
        subpass,
        dependency,
    };

    render_pass_ = vk::raii::RenderPass{device_, create_info};
}

// ---------------------------------------------------------------------------
// Framebuffers
// ---------------------------------------------------------------------------

void Engine::create_framebuffers() {
    framebuffers_.clear();
    for (const auto& image_view : swapchain_image_views_) {
        std::array<vk::ImageView, 2> attachments = {*image_view, *depth_image_view_};

        vk::FramebufferCreateInfo create_info{
            {},
            *render_pass_,
            attachments,
            swapchain_extent_.width,
            swapchain_extent_.height,
            1,
        };

        framebuffers_.emplace_back(device_, create_info);
    }
}

// ---------------------------------------------------------------------------
// Command pool & buffers
// ---------------------------------------------------------------------------

void Engine::create_command_pool() {
    vk::CommandPoolCreateInfo pool_info{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        graphics_family_index_,
    };
    command_pool_ = vk::raii::CommandPool{device_, pool_info};

    vk::CommandBufferAllocateInfo alloc_info{
        *command_pool_,
        vk::CommandBufferLevel::ePrimary,
        static_cast<uint32_t>(swapchain_images_.size()),
    };
    command_buffers_ = device_.allocateCommandBuffers(alloc_info);
}

// ---------------------------------------------------------------------------
// Sync objects
// ---------------------------------------------------------------------------

void Engine::create_sync_objects() {
    auto count = static_cast<uint32_t>(swapchain_images_.size());
    image_available_.reserve(count);
    render_finished_.reserve(count);
    in_flight_fences_.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        image_available_.emplace_back(device_, vk::SemaphoreCreateInfo{});
        render_finished_.emplace_back(device_, vk::SemaphoreCreateInfo{});
        in_flight_fences_.emplace_back(device_, vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled});
    }
    spdlog::info("Created {} sync object sets for {} swapchain images", count, count);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

uint32_t Engine::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const {
    auto mem_props = physical_device_.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace steel

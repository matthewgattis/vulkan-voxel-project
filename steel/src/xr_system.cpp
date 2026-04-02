#include <steel/xr_system.hpp>

#include <spdlog/spdlog.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace steel {

// Static members
XrInstance XrSystem::s_xr_instance_ = XR_NULL_HANDLE;
XrSystemId XrSystem::s_xr_system_id_ = XR_NULL_SYSTEM_ID;

// Helper to check XR results
static void xr_check(XrResult result, const char* msg) {
    if (XR_FAILED(result)) {
        throw std::runtime_error(std::string(msg) + " (XrResult: " +
                                 std::to_string(static_cast<int>(result)) + ")");
    }
}

// ---------------------------------------------------------------------------
// query_requirements (static, call before Vulkan instance)
// ---------------------------------------------------------------------------

// Helper to split a space-delimited extension string into a vector of strings.
static std::vector<std::string> split_extensions(const std::string& str) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < str.size()) {
        size_t end = str.find(' ', start);
        if (end == std::string::npos) end = str.size();
        if (end > start) result.emplace_back(str.substr(start, end - start));
        start = end + 1;
    }
    return result;
}

std::optional<XrVulkanRequirements> XrSystem::query_requirements() {
    // Create XR instance with v1 Vulkan enable extension
    const char* extensions[] = {"XR_KHR_vulkan_enable"};

    XrInstanceCreateInfo instance_ci{};
    instance_ci.type = XR_TYPE_INSTANCE_CREATE_INFO;
    std::strncpy(instance_ci.applicationInfo.applicationName, "VoxelVR",
                 XR_MAX_APPLICATION_NAME_SIZE);
    instance_ci.applicationInfo.applicationVersion = 1;
    std::strncpy(instance_ci.applicationInfo.engineName, "steel",
                 XR_MAX_ENGINE_NAME_SIZE);
    instance_ci.applicationInfo.engineVersion = 1;
    instance_ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    instance_ci.enabledExtensionCount = 1;
    instance_ci.enabledExtensionNames = extensions;

    XrResult result = xrCreateInstance(&instance_ci, &s_xr_instance_);
    if (XR_FAILED(result)) {
        spdlog::info("No OpenXR runtime available (result: {}), proceeding desktop-only",
                     static_cast<int>(result));
        s_xr_instance_ = XR_NULL_HANDLE;
        return std::nullopt;
    }

    spdlog::info("OpenXR instance created");

    // Get system (HMD)
    XrSystemGetInfo system_info{};
    system_info.type = XR_TYPE_SYSTEM_GET_INFO;
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    result = xrGetSystem(s_xr_instance_, &system_info, &s_xr_system_id_);
    if (XR_FAILED(result)) {
        spdlog::info("No HMD found (result: {}), proceeding desktop-only",
                     static_cast<int>(result));
        xrDestroyInstance(s_xr_instance_);
        s_xr_instance_ = XR_NULL_HANDLE;
        return std::nullopt;
    }

    spdlog::info("OpenXR HMD system found (id: {})", s_xr_system_id_);

    // Get Vulkan graphics requirements (v1)
    PFN_xrGetVulkanGraphicsRequirementsKHR getReqs = nullptr;
    xr_check(xrGetInstanceProcAddr(s_xr_instance_, "xrGetVulkanGraphicsRequirementsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getReqs)),
             "Failed to get xrGetVulkanGraphicsRequirementsKHR");

    XrGraphicsRequirementsVulkanKHR gfx_reqs{};
    gfx_reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
    xr_check(getReqs(s_xr_instance_, s_xr_system_id_, &gfx_reqs),
             "xrGetVulkanGraphicsRequirementsKHR failed");

    spdlog::info("XR Vulkan requirements: min API {}.{}.{}, max API {}.{}.{}",
                 XR_VERSION_MAJOR(gfx_reqs.minApiVersionSupported),
                 XR_VERSION_MINOR(gfx_reqs.minApiVersionSupported),
                 XR_VERSION_PATCH(gfx_reqs.minApiVersionSupported),
                 XR_VERSION_MAJOR(gfx_reqs.maxApiVersionSupported),
                 XR_VERSION_MINOR(gfx_reqs.maxApiVersionSupported),
                 XR_VERSION_PATCH(gfx_reqs.maxApiVersionSupported));

    // Get required Vulkan instance extensions (v1 returns space-delimited string)
    PFN_xrGetVulkanInstanceExtensionsKHR getInstExts = nullptr;
    xr_check(xrGetInstanceProcAddr(s_xr_instance_, "xrGetVulkanInstanceExtensionsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getInstExts)),
             "Failed to get xrGetVulkanInstanceExtensionsKHR");

    uint32_t inst_ext_size = 0;
    xr_check(getInstExts(s_xr_instance_, s_xr_system_id_, 0, &inst_ext_size, nullptr),
             "xrGetVulkanInstanceExtensionsKHR size query failed");

    XrVulkanRequirements reqs;

    if (inst_ext_size > 0) {
        std::string inst_ext_str(inst_ext_size, '\0');
        xr_check(getInstExts(s_xr_instance_, s_xr_system_id_, inst_ext_size,
                             &inst_ext_size, inst_ext_str.data()),
                 "xrGetVulkanInstanceExtensionsKHR failed");
        reqs.instance_extensions = split_extensions(inst_ext_str);
        for (const auto& ext : reqs.instance_extensions) {
            spdlog::info("XR requires VkInstance extension: {}", ext);
        }
    }

    // Get required Vulkan device extensions (v1)
    PFN_xrGetVulkanDeviceExtensionsKHR getDevExts = nullptr;
    xr_check(xrGetInstanceProcAddr(s_xr_instance_, "xrGetVulkanDeviceExtensionsKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getDevExts)),
             "Failed to get xrGetVulkanDeviceExtensionsKHR");

    uint32_t dev_ext_size = 0;
    xr_check(getDevExts(s_xr_instance_, s_xr_system_id_, 0, &dev_ext_size, nullptr),
             "xrGetVulkanDeviceExtensionsKHR size query failed");

    if (dev_ext_size > 0) {
        std::string dev_ext_str(dev_ext_size, '\0');
        xr_check(getDevExts(s_xr_instance_, s_xr_system_id_, dev_ext_size,
                            &dev_ext_size, dev_ext_str.data()),
                 "xrGetVulkanDeviceExtensionsKHR failed");
        reqs.device_extensions = split_extensions(dev_ext_str);
        for (const auto& ext : reqs.device_extensions) {
            spdlog::info("XR requires VkDevice extension: {}", ext);
        }
    }

    // Cap Vulkan API version to what the XR runtime supports
    reqs.max_vulkan_api_version = VK_MAKE_API_VERSION(0,
        XR_VERSION_MAJOR(gfx_reqs.maxApiVersionSupported),
        XR_VERSION_MINOR(gfx_reqs.maxApiVersionSupported),
        XR_VERSION_PATCH(gfx_reqs.maxApiVersionSupported));

    return reqs;
}

VkPhysicalDevice XrSystem::query_physical_device(VkInstance vk_instance) {
    if (s_xr_instance_ == XR_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    PFN_xrGetVulkanGraphicsDeviceKHR getDevice = nullptr;
    xr_check(xrGetInstanceProcAddr(s_xr_instance_, "xrGetVulkanGraphicsDeviceKHR",
                                   reinterpret_cast<PFN_xrVoidFunction*>(&getDevice)),
             "Failed to get xrGetVulkanGraphicsDeviceKHR");

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    xr_check(getDevice(s_xr_instance_, s_xr_system_id_, vk_instance, &physical_device),
             "xrGetVulkanGraphicsDeviceKHR failed");

    spdlog::info("XR runtime selected physical device: {:p}",
                 static_cast<void*>(physical_device));
    return physical_device;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

XrSystem::XrSystem(VkInstance instance, VkPhysicalDevice physical_device,
                   VkDevice device, uint32_t queue_family, uint32_t queue_index,
                   VmaAllocator allocator, vk::Format color_format, vk::Format depth_format,
                   const vk::raii::Device& raii_device)
    : xr_instance_(s_xr_instance_)
    , xr_system_id_(s_xr_system_id_)
    , color_format_(color_format)
    , depth_format_(depth_format)
    , vk_device_(device)
    , allocator_(allocator)
    , raii_device_(&raii_device)
{
    // Clear static state (we've taken ownership)
    s_xr_instance_ = XR_NULL_HANDLE;
    s_xr_system_id_ = XR_NULL_SYSTEM_ID;

    // Create session (v1 binding type)
    XrGraphicsBindingVulkanKHR binding{};
    binding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
    binding.instance = instance;
    binding.physicalDevice = physical_device;
    binding.device = device;
    binding.queueFamilyIndex = queue_family;
    binding.queueIndex = queue_index;

    XrSessionCreateInfo session_ci{};
    session_ci.type = XR_TYPE_SESSION_CREATE_INFO;
    session_ci.next = &binding;
    session_ci.systemId = xr_system_id_;

    xr_check(xrCreateSession(xr_instance_, &session_ci, &xr_session_),
             "xrCreateSession failed");
    spdlog::info("OpenXR session created");

    // Create reference space (LOCAL = seated, origin at startup position)
    XrReferenceSpaceCreateInfo space_ci{};
    space_ci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    space_ci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_ci.poseInReferenceSpace.orientation.w = 1.0f; // identity

    xr_check(xrCreateReferenceSpace(xr_session_, &space_ci, &xr_space_),
             "xrCreateReferenceSpace failed");

    // Initialize view storage
    for (auto& v : xr_views_) {
        v.type = XR_TYPE_VIEW;
        v.next = nullptr;
    }

    create_render_pass();
    create_swapchains();

    spdlog::info("OpenXR system fully initialized");
}

XrSystem::~XrSystem() {
    // Destroy eye swapchains
    for (auto& eye : eye_swapchains_) {
        eye.framebuffers.clear();
        eye.image_views.clear();
        if (eye.handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(eye.handle);
        }
    }

    // Depth images are cleaned up by VmaImage destructors

    if (xr_space_ != XR_NULL_HANDLE) xrDestroySpace(xr_space_);
    if (xr_session_ != XR_NULL_HANDLE) xrDestroySession(xr_session_);
    if (xr_instance_ != XR_NULL_HANDLE) xrDestroyInstance(xr_instance_);

    spdlog::info("OpenXR system destroyed");
}

// ---------------------------------------------------------------------------
// Swapchain creation
// ---------------------------------------------------------------------------

void XrSystem::create_swapchains() {
    // Query view configuration
    uint32_t view_count = 0;
    xr_check(xrEnumerateViewConfigurationViews(xr_instance_, xr_system_id_,
                                                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                0, &view_count, nullptr),
             "xrEnumerateViewConfigurationViews count");

    if (view_count != 2) {
        throw std::runtime_error("Expected 2 stereo views, got " + std::to_string(view_count));
    }

    std::array<XrViewConfigurationView, 2> config_views;
    for (auto& cv : config_views) {
        cv.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        cv.next = nullptr;
    }

    xr_check(xrEnumerateViewConfigurationViews(xr_instance_, xr_system_id_,
                                                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                2, &view_count, config_views.data()),
             "xrEnumerateViewConfigurationViews");

    eye_extent_ = vk::Extent2D{
        config_views[0].recommendedImageRectWidth,
        config_views[0].recommendedImageRectHeight};

    spdlog::info("XR eye resolution: {}x{}", eye_extent_.width, eye_extent_.height);

    // Create swapchain for each eye
    for (uint32_t eye = 0; eye < 2; ++eye) {
        XrSwapchainCreateInfo swapchain_ci{};
        swapchain_ci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swapchain_ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_ci.format = static_cast<int64_t>(VK_FORMAT_B8G8R8A8_SRGB);
        swapchain_ci.sampleCount = 1;
        swapchain_ci.width = eye_extent_.width;
        swapchain_ci.height = eye_extent_.height;
        swapchain_ci.faceCount = 1;
        swapchain_ci.arraySize = 1;
        swapchain_ci.mipCount = 1;

        xr_check(xrCreateSwapchain(xr_session_, &swapchain_ci, &eye_swapchains_[eye].handle),
                 "xrCreateSwapchain failed");

        // Get swapchain images
        uint32_t image_count = 0;
        xr_check(xrEnumerateSwapchainImages(eye_swapchains_[eye].handle, 0, &image_count, nullptr),
                 "xrEnumerateSwapchainImages count");

        eye_swapchains_[eye].images.resize(image_count);
        for (auto& img : eye_swapchains_[eye].images) {
            img.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
            img.next = nullptr;
        }

        xr_check(xrEnumerateSwapchainImages(
                     eye_swapchains_[eye].handle, image_count, &image_count,
                     reinterpret_cast<XrSwapchainImageBaseHeader*>(eye_swapchains_[eye].images.data())),
                 "xrEnumerateSwapchainImages");

        spdlog::info("XR eye {} swapchain: {} images", eye, image_count);

        // Create image views and framebuffers for each swapchain image
        for (uint32_t i = 0; i < image_count; ++i) {
            vk::ImageViewCreateInfo view_ci{
                .image = eye_swapchains_[eye].images[i].image,
                .viewType = vk::ImageViewType::e2D,
                .format = vk::Format::eB8G8R8A8Srgb,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0, .levelCount = 1,
                    .baseArrayLayer = 0, .layerCount = 1,
                },
            };
            eye_swapchains_[eye].image_views.emplace_back(*raii_device_, view_ci);
        }
    }

    // Create shared depth resources and framebuffers
    create_depth_resources();

    // Create framebuffers (one per swapchain image per eye)
    for (uint32_t eye = 0; eye < 2; ++eye) {
        for (uint32_t i = 0; i < eye_swapchains_[eye].images.size(); ++i) {
            std::array<vk::ImageView, 2> attachments = {
                *eye_swapchains_[eye].image_views[i],
                *depth_image_views_[eye],
            };

            vk::FramebufferCreateInfo fb_ci{
                .renderPass = *render_pass_,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = eye_extent_.width,
                .height = eye_extent_.height,
                .layers = 1,
            };
            eye_swapchains_[eye].framebuffers.emplace_back(*raii_device_, fb_ci);
        }
    }
}

// ---------------------------------------------------------------------------
// Render pass (XR eyes -- final layout is COLOR_ATTACHMENT_OPTIMAL)
// ---------------------------------------------------------------------------

void XrSystem::create_render_pass() {
    // XR render pass: similar to the scene render pass but the color attachment
    // stays in COLOR_ATTACHMENT_OPTIMAL (the XR runtime handles final transitions)
    vk::AttachmentDescription color_attachment{
        .format = vk::Format::eB8G8R8A8Srgb,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
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
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                         vk::AccessFlagBits::eDepthStencilAttachmentWrite,
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

    render_pass_ = vk::raii::RenderPass{*raii_device_, create_info};
}

// ---------------------------------------------------------------------------
// Depth resources (one per eye)
// ---------------------------------------------------------------------------

void XrSystem::create_depth_resources() {
    for (uint32_t eye = 0; eye < 2; ++eye) {
        VkImageCreateInfo image_ci{};
        image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_ci.imageType = VK_IMAGE_TYPE_2D;
        image_ci.format = static_cast<VkFormat>(depth_format_);
        image_ci.extent = {eye_extent_.width, eye_extent_.height, 1};
        image_ci.mipLevels = 1;
        image_ci.arrayLayers = 1;
        image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
        image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VmaAllocationCreateInfo alloc_ci{};
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        VkImage raw_image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        vmaCreateImage(allocator_, &image_ci, &alloc_ci, &raw_image, &allocation, nullptr);

        depth_images_[eye].allocator = allocator_;
        depth_images_[eye].image = raw_image;
        depth_images_[eye].allocation = allocation;

        vk::ImageViewCreateInfo view_ci{
            .image = raw_image,
            .viewType = vk::ImageViewType::e2D,
            .format = depth_format_,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eDepth,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1,
            },
        };
        depth_image_views_[eye] = vk::raii::ImageView{*raii_device_, view_ci};
    }
}

// ---------------------------------------------------------------------------
// Event polling
// ---------------------------------------------------------------------------

void XrSystem::poll_events() {
    XrEventDataBuffer event{};
    event.type = XR_TYPE_EVENT_DATA_BUFFER;

    while (xrPollEvent(xr_instance_, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* state_event = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
            session_state_ = state_event->state;

            spdlog::info("XR session state changed: {}", static_cast<int>(session_state_));

            if (session_state_ == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo begin_info{};
                begin_info.type = XR_TYPE_SESSION_BEGIN_INFO;
                begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                xr_check(xrBeginSession(xr_session_, &begin_info), "xrBeginSession failed");
                session_running_ = true;
                spdlog::info("XR session started");
            } else if (session_state_ == XR_SESSION_STATE_STOPPING) {
                xr_check(xrEndSession(xr_session_), "xrEndSession failed");
                session_running_ = false;
                spdlog::info("XR session stopped");
            }
        }

        // Reset for next event
        event.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

XrFrameState XrSystem::wait_and_begin_frame(const glm::vec3& body_position, float body_yaw) {
    XrFrameState result{};

    XrFrameWaitInfo wait_info{};
    wait_info.type = XR_TYPE_FRAME_WAIT_INFO;
    ::XrFrameState xr_fs{};
    xr_fs.type = XR_TYPE_FRAME_STATE;

    xr_check(xrWaitFrame(xr_session_, &wait_info, &xr_fs), "xrWaitFrame failed");

    result.should_render = xr_fs.shouldRender == XR_TRUE;
    result.predicted_display_time = xr_fs.predictedDisplayTime;

    XrFrameBeginInfo begin_info{};
    begin_info.type = XR_TYPE_FRAME_BEGIN_INFO;
    xr_check(xrBeginFrame(xr_session_, &begin_info), "xrBeginFrame failed");

    if (result.should_render) {
        // Locate views (eye poses)
        XrViewLocateInfo locate_info{};
        locate_info.type = XR_TYPE_VIEW_LOCATE_INFO;
        locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate_info.displayTime = result.predicted_display_time;
        locate_info.space = xr_space_;

        XrViewState view_state{};
        view_state.type = XR_TYPE_VIEW_STATE;

        uint32_t view_count = 2;
        xr_check(xrLocateViews(xr_session_, &locate_info, &view_state,
                                2, &view_count, xr_views_.data()),
                 "xrLocateViews failed");

        bool position_valid = (view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0;
        bool orientation_valid = (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0;

        // Log tracking state and first pose once
        static bool first_render = true;
        if (first_render) {
            spdlog::info("XR view state flags: 0x{:x} (pos_valid={}, ori_valid={})",
                         view_state.viewStateFlags, position_valid, orientation_valid);
            auto& p = xr_views_[0].pose;
            spdlog::info("XR eye 0 pose: pos=({:.3f},{:.3f},{:.3f}) ori=({:.3f},{:.3f},{:.3f},{:.3f})",
                         p.position.x, p.position.y, p.position.z,
                         p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w);
            auto& f = xr_views_[0].fov;
            spdlog::info("XR eye 0 fov: L={:.2f} R={:.2f} U={:.2f} D={:.2f} (deg)",
                         glm::degrees(f.angleLeft), glm::degrees(f.angleRight),
                         glm::degrees(f.angleUp), glm::degrees(f.angleDown));
            spdlog::info("XR body position: ({:.1f},{:.1f},{:.1f})",
                         body_position.x, body_position.y, body_position.z);
            first_render = false;
        }

        for (uint32_t eye = 0; eye < 2; ++eye) {
            result.eyes[eye].pose = xr_views_[eye].pose;
            result.eyes[eye].fov = xr_views_[eye].fov;
            result.eyes[eye].view = xr_pose_to_view_matrix(xr_views_[eye].pose, body_position, body_yaw);
            result.eyes[eye].projection = xr_fov_to_projection(xr_views_[eye].fov, 0.1f, 500.0f);
        }
    }

    return result;
}

void XrSystem::end_frame(const XrFrameState& state) {
    XrFrameEndInfo end_info{};
    end_info.type = XR_TYPE_FRAME_END_INFO;
    end_info.displayTime = state.predicted_display_time;
    end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

    XrCompositionLayerProjection projection_layer{};
    const XrCompositionLayerBaseHeader* layers_ptr = nullptr;

    if (state.should_render) {
        // Build projection views
        for (uint32_t eye = 0; eye < 2; ++eye) {
            projection_views_[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            projection_views_[eye].next = nullptr;
            projection_views_[eye].pose = state.eyes[eye].pose;
            projection_views_[eye].fov = state.eyes[eye].fov;
            projection_views_[eye].subImage.swapchain = eye_swapchains_[eye].handle;
            projection_views_[eye].subImage.imageRect.offset = {0, 0};
            projection_views_[eye].subImage.imageRect.extent = {
                static_cast<int32_t>(eye_extent_.width),
                static_cast<int32_t>(eye_extent_.height)};
            projection_views_[eye].subImage.imageArrayIndex = 0;
        }

        projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        projection_layer.space = xr_space_;
        projection_layer.viewCount = 2;
        projection_layer.views = projection_views_.data();

        layers_ptr = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection_layer);
        end_info.layerCount = 1;
        end_info.layers = &layers_ptr;
    }

    xr_check(xrEndFrame(xr_session_, &end_info), "xrEndFrame failed");
}

// ---------------------------------------------------------------------------
// Per-eye rendering
// ---------------------------------------------------------------------------

void XrSystem::begin_eye_render(const vk::raii::CommandBuffer& cmd, uint32_t eye) {
    auto& sc = eye_swapchains_[eye];

    // Acquire swapchain image
    XrSwapchainImageAcquireInfo acquire_info{};
    acquire_info.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    xr_check(xrAcquireSwapchainImage(sc.handle, &acquire_info, &sc.current_index),
             "xrAcquireSwapchainImage failed");

    // Wait for it to be ready
    XrSwapchainImageWaitInfo wait_info{};
    wait_info.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    wait_info.timeout = XR_INFINITE_DURATION;
    xr_check(xrWaitSwapchainImage(sc.handle, &wait_info),
             "xrWaitSwapchainImage failed");

    // Begin render pass
    std::array<vk::ClearValue, 2> clear_values = {
        vk::ClearValue{vk::ClearColorValue{std::array<float, 4>{0.53f, 0.71f, 0.92f, 1.0f}}},
        vk::ClearValue{vk::ClearDepthStencilValue{.depth = 1.0f, .stencil = 0}},
    };

    vk::RenderPassBeginInfo rp_info{
        .renderPass = *render_pass_,
        .framebuffer = *sc.framebuffers[sc.current_index],
        .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = eye_extent_},
        .clearValueCount = static_cast<uint32_t>(clear_values.size()),
        .pClearValues = clear_values.data(),
    };

    cmd.beginRenderPass(rp_info, vk::SubpassContents::eInline);

    vk::Viewport viewport{
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(eye_extent_.width),
        .height = static_cast<float>(eye_extent_.height),
        .minDepth = 0.0f, .maxDepth = 1.0f};
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{.offset = {0, 0}, .extent = eye_extent_};
    cmd.setScissor(0, scissor);
}

void XrSystem::end_eye_render(const vk::raii::CommandBuffer& cmd, uint32_t eye) {
    cmd.endRenderPass();

    // Release swapchain image
    XrSwapchainImageReleaseInfo release_info{};
    release_info.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    xr_check(xrReleaseSwapchainImage(eye_swapchains_[eye].handle, &release_info),
             "xrReleaseSwapchainImage failed");
}

// ---------------------------------------------------------------------------
// Coordinate transforms
// ---------------------------------------------------------------------------

glm::mat4 XrSystem::xr_pose_to_view_matrix(const XrPosef& pose,
                                             const glm::vec3& body_position,
                                             float body_yaw) const {
    // XR quaternion to glm (XR is xyzw, glm constructor is wxyz)
    glm::quat q(pose.orientation.w, pose.orientation.x,
                pose.orientation.y, pose.orientation.z);
    glm::vec3 xr_pos(pose.position.x, pose.position.y, pose.position.z);

    // XR local eye transform (in XR Y-up space)
    glm::mat4 eye_local = glm::translate(glm::mat4(1.0f), xr_pos) * glm::mat4_cast(q);

    // Coordinate transform: XR Y-up right-handed → engine Z-up
    // XR: X=right, Y=up, Z=back
    // Engine: X=right, Y=forward, Z=up
    // Transform: rotate +90 degrees around X axis
    //   Y-up → Z-up, Z-back → -Y (engine forward is +Y)
    static const glm::mat4 xr_to_engine =
        glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));

    // Body yaw: horizontal rotation in engine space (around Z-up axis).
    // This lets mouse yaw rotate the entire XR reference frame.
    glm::mat4 body_rot = glm::rotate(glm::mat4(1.0f), body_yaw, glm::vec3(0.0f, 0.0f, 1.0f));

    // Compose: body position + body yaw + coordinate-transformed eye pose
    glm::mat4 eye_world = glm::translate(glm::mat4(1.0f), body_position) *
                           body_rot * xr_to_engine * eye_local;

    return glm::inverse(eye_world);
}

glm::mat4 XrSystem::xr_fov_to_projection(const XrFovf& fov,
                                           float near_plane, float far_plane) const {
    float left   = near_plane * std::tan(fov.angleLeft);
    float right  = near_plane * std::tan(fov.angleRight);
    float up     = near_plane * std::tan(fov.angleUp);
    float down   = near_plane * std::tan(fov.angleDown);

    glm::mat4 proj = glm::frustum(left, right, down, up, near_plane, far_plane);
    // Vulkan Y-flip: negate the entire Y row (both scale and offset).
    // For symmetric frustum (desktop), proj[2][1] is 0 and only [1][1] matters.
    // For asymmetric frustum (XR), proj[2][1] is non-zero and must also be
    // negated — otherwise the vertical projection center is inverted, causing
    // the scene to warp as the head rotates.
    proj[1][1] *= -1.0f;
    proj[2][1] *= -1.0f;
    return proj;
}

} // namespace steel

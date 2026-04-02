#pragma once

#include <steel/swapchain.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace steel {

struct XrVulkanRequirements {
    std::vector<std::string> instance_extensions;
    std::vector<std::string> device_extensions;
    uint32_t max_vulkan_api_version = 0;
};

struct XrFrameState {
    bool should_render = false;
    XrTime predicted_display_time = 0;

    struct EyeView {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        XrPosef pose{};
        XrFovf fov{};
    };
    std::array<EyeView, 2> eyes;
};

class XrSystem {
public:
    // Call BEFORE creating Vulkan instance. Returns requirements or nullopt if no HMD.
    // On success, stores internal XR state for use by the constructor.
    static std::optional<XrVulkanRequirements> query_requirements();

    // Call AFTER VkInstance creation to get the physical device the XR runtime requires.
    // Pass this as EngineConfig::physical_device_query.
    static VkPhysicalDevice query_physical_device(VkInstance vk_instance);

    // Call AFTER Vulkan device creation. Takes ownership of the static XR state
    // from query_requirements().
    XrSystem(VkInstance instance, VkPhysicalDevice physical_device,
             VkDevice device, uint32_t queue_family, uint32_t queue_index,
             VmaAllocator allocator, vk::Format color_format, vk::Format depth_format,
             const vk::raii::Device& raii_device);
    ~XrSystem();

    XrSystem(const XrSystem&) = delete;
    XrSystem& operator=(const XrSystem&) = delete;
    XrSystem(XrSystem&&) = delete;
    XrSystem& operator=(XrSystem&&) = delete;

    bool active() const { return session_running_; }

    // Poll XR events (session state changes). Call each frame.
    void poll_events();

    // Frame lifecycle. body_yaw is the horizontal rotation (radians around Z-up)
    // applied to the XR reference frame, allowing mouse yaw to rotate the VR view.
    XrFrameState wait_and_begin_frame(const glm::vec3& body_position, float body_yaw = 0.0f);
    void end_frame(const XrFrameState& state);

    // Per-eye rendering. Call between wait_and_begin_frame and end_frame.
    void begin_eye_render(const vk::raii::CommandBuffer& cmd, uint32_t eye);
    void end_eye_render(const vk::raii::CommandBuffer& cmd, uint32_t eye);

    vk::Extent2D eye_extent() const { return eye_extent_; }
    const vk::raii::RenderPass& render_pass() const { return render_pass_; }

private:
    void create_session();
    void create_swapchains();
    void create_render_pass();
    void create_depth_resources();

    glm::mat4 xr_pose_to_view_matrix(const XrPosef& pose,
                                      const glm::vec3& body_position,
                                      float body_yaw) const;
    glm::mat4 xr_fov_to_projection(const XrFovf& fov,
                                    float near_plane, float far_plane) const;

    // OpenXR core
    XrInstance xr_instance_ = XR_NULL_HANDLE;
    XrSystemId xr_system_id_ = XR_NULL_SYSTEM_ID;
    XrSession xr_session_ = XR_NULL_HANDLE;
    XrSpace xr_space_ = XR_NULL_HANDLE;
    XrSessionState session_state_ = XR_SESSION_STATE_UNKNOWN;
    bool session_running_ = false;

    // Per-eye swapchains
    struct EyeSwapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
        std::vector<XrSwapchainImageVulkanKHR> images;
        std::vector<vk::raii::ImageView> image_views;
        std::vector<vk::raii::Framebuffer> framebuffers;
        uint32_t current_index = 0;
    };
    std::array<EyeSwapchain, 2> eye_swapchains_;
    std::array<XrView, 2> xr_views_;
    std::array<XrCompositionLayerProjectionView, 2> projection_views_;

    // Shared render resources
    vk::Extent2D eye_extent_;
    vk::Format color_format_;
    vk::Format depth_format_;
    vk::raii::RenderPass render_pass_{nullptr};

    // Per-eye depth buffers (shared across swapchain images since only one eye renders at a time)
    std::array<VmaImage, 2> depth_images_;
    std::array<vk::raii::ImageView, 2> depth_image_views_{
        vk::raii::ImageView{nullptr}, vk::raii::ImageView{nullptr}};

    // Vulkan handles (non-owning)
    VkDevice vk_device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    const vk::raii::Device* raii_device_ = nullptr;

public:
    // True if query_requirements() found an HMD and static state is pending
    static bool has_pending_session() { return s_xr_instance_ != XR_NULL_HANDLE; }

private:
    static XrInstance s_xr_instance_;
    static XrSystemId s_xr_system_id_;
};

} // namespace steel

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <glm/vec2.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace steel {

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class Engine {
public:
    Engine(std::string_view title, uint32_t width, uint32_t height);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Returns true if the application should keep running.
    bool poll_events();

    // Frame rendering interface. begin_frame returns the command buffer
    // for recording into, or nullptr if the swapchain image could not
    // be acquired (e.g. window minimized). end_frame submits and presents.
    const vk::raii::CommandBuffer* begin_frame();
    void end_frame();

    void wait_idle();

    // Accessors
    const vk::raii::Device&     device()      const { return device_; }
    const vk::raii::RenderPass& render_pass() const { return render_pass_; }
    vk::Extent2D                extent()      const { return swapchain_extent_; }
    vk::Format                  color_format() const { return surface_format_.format; }
    vk::Format                  depth_format() const { return depth_format_; }
    const vk::raii::PhysicalDevice& physical_device() const { return physical_device_; }
    const vk::raii::Queue&      graphics_queue() const { return graphics_queue_; }
    const vk::raii::CommandPool& command_pool()  const { return command_pool_; }
    uint32_t                    graphics_family() const { return graphics_family_index_; }
    uint32_t                    current_frame()   const { return current_frame_; }

    // Input state
    const bool* keyboard_state() const;
    float mouse_dx() const { return mouse_dx_; }
    float mouse_dy() const { return mouse_dy_; }
    float delta_time() const { return delta_time_; }

private:
    void create_instance(std::string_view title);
    void create_surface();
    void pick_physical_device();
    void create_device();
    void create_swapchain();
    void create_depth_resources();
    void create_offscreen_target();
    void create_render_pass();
    void create_offscreen_framebuffer();
    void create_fxaa_render_pass();
    void create_fxaa_framebuffers();
    void create_command_pool();
    void create_sync_objects();
    void create_fxaa_descriptors();
    void create_fxaa_pipeline();
    void update_fxaa_descriptor();
    void recreate_swapchain();

    bool is_device_suitable(const vk::raii::PhysicalDevice& dev) const;
    vk::Format find_depth_format(const vk::raii::PhysicalDevice& dev) const;
    uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;

    // SDL
    SDL_Window* window_ = nullptr;

    // Vulkan core (declaration order matters for destruction)
    vk::raii::Context       context_;
    vk::raii::Instance      instance_       {nullptr};
    vk::raii::SurfaceKHR    surface_        {nullptr};
    vk::raii::PhysicalDevice physical_device_{nullptr};
    vk::raii::Device        device_         {nullptr};
    vk::raii::Queue         graphics_queue_ {nullptr};
    vk::raii::Queue         present_queue_  {nullptr};

    uint32_t graphics_family_index_ = 0;
    uint32_t present_family_index_  = 0;

    // Swapchain
    vk::SurfaceFormatKHR             surface_format_;
    vk::Extent2D                     swapchain_extent_;
    vk::raii::SwapchainKHR           swapchain_     {nullptr};
    std::vector<vk::Image>           swapchain_images_;
    std::vector<vk::raii::ImageView> swapchain_image_views_;

    // Depth buffer
    vk::Format                   depth_format_ = vk::Format::eD32Sfloat;
    vk::raii::Image              depth_image_      {nullptr};
    vk::raii::DeviceMemory       depth_memory_     {nullptr};
    vk::raii::ImageView          depth_image_view_ {nullptr};

    // Offscreen render target (scene renders here, FXAA reads from it)
    vk::raii::Image              offscreen_image_      {nullptr};
    vk::raii::DeviceMemory       offscreen_memory_     {nullptr};
    vk::raii::ImageView          offscreen_image_view_ {nullptr};

    // Scene render pass & offscreen framebuffer
    vk::raii::RenderPass                render_pass_  {nullptr};
    vk::raii::Framebuffer               offscreen_framebuffer_ {nullptr};

    // FXAA render pass & per-swapchain framebuffers
    vk::raii::RenderPass                fxaa_render_pass_ {nullptr};
    std::vector<vk::raii::Framebuffer>  fxaa_framebuffers_;

    // FXAA pipeline resources
    vk::raii::DescriptorSetLayout       fxaa_descriptor_set_layout_ {nullptr};
    vk::raii::DescriptorPool            fxaa_descriptor_pool_ {nullptr};
    vk::raii::DescriptorSet             fxaa_descriptor_set_ {nullptr};
    vk::raii::Sampler                   fxaa_sampler_ {nullptr};
    vk::raii::PipelineLayout            fxaa_pipeline_layout_ {nullptr};
    vk::raii::Pipeline                  fxaa_pipeline_ {nullptr};

    // Commands
    vk::raii::CommandPool                command_pool_ {nullptr};
    std::vector<vk::raii::CommandBuffer> command_buffers_;

    // Synchronization (per-frame-in-flight)
    std::vector<vk::raii::Semaphore> image_available_;
    std::vector<vk::raii::Semaphore> render_finished_;
    std::vector<vk::raii::Fence>     in_flight_fences_;

    uint32_t current_frame_ = 0;
    uint32_t current_image_index_ = 0;
    bool framebuffer_resized_ = false;

    // Input state
    float mouse_dx_ = 0.0f;
    float mouse_dy_ = 0.0f;
    float delta_time_ = 0.0f;
    uint64_t last_frame_time_ = 0;
    bool mouse_captured_ = false;
    bool mouse_capture_first_frame_ = false;
};

} // namespace steel

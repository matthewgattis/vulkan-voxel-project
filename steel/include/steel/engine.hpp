#pragma once

#include <steel/fxaa_pass.hpp>
#include <steel/imgui_pass.hpp>
#include <steel/swapchain.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace steel {

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class Engine {
public:
    Engine(std::string_view title);
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

    // ImGui
    void imgui_begin()  { imgui_pass_.begin(); }
    void imgui_end()    { imgui_pass_.end(); }
    bool imgui_enabled() const { return imgui_pass_.enabled(); }
    void set_imgui_enabled(bool enabled) { imgui_pass_.set_enabled(enabled); }

    // Accessors
    const vk::raii::Device&     device()      const { return device_; }
    const vk::raii::RenderPass& render_pass() const { return swapchain_.render_pass(); }
    vk::Extent2D                extent()      const { return swapchain_.extent(); }
    vk::Format                  color_format() const { return swapchain_.color_format(); }
    vk::Format                  depth_format() const { return swapchain_.depth_format(); }
    const vk::raii::PhysicalDevice& physical_device() const { return physical_device_; }
    const vk::raii::Queue&      graphics_queue() const { return graphics_queue_; }
    const vk::raii::CommandPool& command_pool()  const { return command_pool_; }
    uint32_t                    graphics_family() const { return graphics_family_index_; }
    uint32_t                    current_frame()   const { return current_frame_; }
    VmaAllocator                allocator()       const { return allocator_; }

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
    void create_allocator();
    void create_command_pool();
    void create_sync_objects();
    void recreate_swapchain();

    bool is_device_suitable(const vk::raii::PhysicalDevice& dev) const;

    // SDL
    SDL_Window* window_ = nullptr;

    // Vulkan core (declaration order matters for destruction)
    vk::raii::Context       context_;
    vk::raii::Instance      instance_       {nullptr};
    vk::raii::SurfaceKHR    surface_        {nullptr};
    vk::raii::PhysicalDevice physical_device_{nullptr};
    vk::raii::Device        device_         {nullptr};
    VmaAllocator            allocator_      = VK_NULL_HANDLE;
    vk::raii::Queue         graphics_queue_ {nullptr};
    vk::raii::Queue         present_queue_  {nullptr};

    uint32_t graphics_family_index_ = 0;
    uint32_t present_family_index_  = 0;

    // Swapchain & render targets
    Swapchain swapchain_;

    // Post-process passes
    FxaaPass  fxaa_pass_;
    ImGuiPass imgui_pass_;

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

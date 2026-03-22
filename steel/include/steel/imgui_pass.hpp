#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <vector>

namespace steel {

class ImGuiPass {
public:
    // Initialize ImGui: render pass, framebuffers, descriptor pool, backends.
    void create(const vk::raii::Instance& instance,
                const vk::raii::PhysicalDevice& physical_device,
                const vk::raii::Device& device,
                uint32_t graphics_family_index,
                const vk::raii::Queue& graphics_queue,
                vk::Format surface_format,
                vk::Extent2D extent,
                const std::vector<vk::raii::ImageView>& swapchain_image_views,
                uint32_t swapchain_image_count,
                SDL_Window* window);

    void shutdown();

    // Call at the start of the frame to begin a new ImGui frame.
    void begin();

    // Call after all ImGui::* calls to finalize draw data.
    void end();

    // Record the ImGui render pass into the command buffer.
    void render(const vk::raii::CommandBuffer& cmd,
                uint32_t image_index,
                vk::Extent2D extent) const;

    // Process an SDL event for ImGui input.
    void process_event(const SDL_Event& event);

    // Recreate extent-dependent resources (framebuffers).
    void recreate(const vk::raii::Device& device,
                  vk::Extent2D extent,
                  const std::vector<vk::raii::ImageView>& swapchain_image_views);

    bool initialized() const { return initialized_; }
    bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    void toggle_enabled() { enabled_ = !enabled_; }

    ImGuiPass() = default;
    ~ImGuiPass();
    ImGuiPass(ImGuiPass&&) = default;
    ImGuiPass& operator=(ImGuiPass&&) = default;

private:
    void create_render_pass(const vk::raii::Device& device, vk::Format surface_format);
    void create_framebuffers(const vk::raii::Device& device,
                             vk::Extent2D extent,
                             const std::vector<vk::raii::ImageView>& swapchain_image_views);

    vk::raii::RenderPass render_pass_{nullptr};
    std::vector<vk::raii::Framebuffer> framebuffers_;
    vk::raii::DescriptorPool descriptor_pool_{nullptr};
    bool initialized_ = false;
    bool enabled_ = true;
};

} // namespace steel

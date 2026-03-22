#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vector>

namespace steel {

class FxaaPass {
public:
    // Creates the FXAA pass: render pass, framebuffers, descriptors, pipeline.
    // swapchain_image_views: the swapchain image views to create framebuffers for.
    // offscreen_image_view: the offscreen color target that FXAA reads from.
    void create(const vk::raii::Device& device,
                vk::Format surface_format,
                vk::Extent2D extent,
                const std::vector<vk::raii::ImageView>& swapchain_image_views,
                const vk::raii::ImageView& offscreen_image_view);

    // Record the FXAA fullscreen pass into the command buffer.
    void apply(const vk::raii::CommandBuffer& cmd,
               uint32_t image_index,
               vk::Extent2D extent) const;

    // Recreate extent-dependent resources (framebuffers + descriptor update).
    void recreate(const vk::raii::Device& device,
                  vk::Extent2D extent,
                  const std::vector<vk::raii::ImageView>& swapchain_image_views,
                  const vk::raii::ImageView& offscreen_image_view);

    // The render pass, needed by Engine for layout transition knowledge.
    const vk::raii::RenderPass& render_pass() const { return render_pass_; }

    FxaaPass() = default;
    FxaaPass(FxaaPass&&) = default;
    FxaaPass& operator=(FxaaPass&&) = default;

private:
    void create_render_pass(const vk::raii::Device& device, vk::Format surface_format);
    void create_framebuffers(const vk::raii::Device& device,
                             vk::Extent2D extent,
                             const std::vector<vk::raii::ImageView>& swapchain_image_views);
    void create_descriptors(const vk::raii::Device& device,
                            const vk::raii::ImageView& offscreen_image_view);
    void create_pipeline(const vk::raii::Device& device);
    void update_descriptor(const vk::raii::Device& device,
                           const vk::raii::ImageView& offscreen_image_view);

    vk::raii::RenderPass render_pass_{nullptr};
    std::vector<vk::raii::Framebuffer> framebuffers_;

    vk::raii::DescriptorSetLayout descriptor_set_layout_{nullptr};
    vk::raii::DescriptorPool descriptor_pool_{nullptr};
    vk::raii::DescriptorSet descriptor_set_{nullptr};
    vk::raii::Sampler sampler_{nullptr};
    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline pipeline_{nullptr};
};

} // namespace steel

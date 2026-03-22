#include <steel/fxaa_pass.hpp>
#include "steel/fxaa_shaders.hpp"

#include <spdlog/spdlog.h>

#include <array>

namespace steel {

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void FxaaPass::create(const vk::raii::Device& device,
                      vk::Format surface_format,
                      vk::Extent2D extent,
                      const std::vector<vk::raii::ImageView>& swapchain_image_views,
                      const vk::raii::ImageView& offscreen_image_view) {
    create_render_pass(device, surface_format);
    create_framebuffers(device, extent, swapchain_image_views);
    create_descriptors(device, offscreen_image_view);
    create_pipeline(device);
}

void FxaaPass::apply(const vk::raii::CommandBuffer& cmd,
                     uint32_t image_index,
                     vk::Extent2D extent) const {
    // Begin FXAA render pass (reads offscreen, writes to swapchain)
    vk::RenderPassBeginInfo fxaa_rp_info{
        .renderPass = *render_pass_,
        .framebuffer = *framebuffers_[image_index],
        .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = extent},
    };

    cmd.beginRenderPass(fxaa_rp_info, vk::SubpassContents::eInline);

    vk::Viewport viewport{
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f, .maxDepth = 1.0f};
    cmd.setViewport(0, viewport);

    vk::Rect2D scissor{.offset = {0, 0}, .extent = extent};
    cmd.setScissor(0, scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline_layout_, 0, *descriptor_set_, {});
    cmd.draw(3, 1, 0, 0);

    cmd.endRenderPass();
}

void FxaaPass::recreate(const vk::raii::Device& device,
                        vk::Extent2D extent,
                        const std::vector<vk::raii::ImageView>& swapchain_image_views,
                        const vk::raii::ImageView& offscreen_image_view) {
    framebuffers_.clear();
    create_framebuffers(device, extent, swapchain_image_views);
    update_descriptor(device, offscreen_image_view);
}

// ---------------------------------------------------------------------------
// FXAA render pass
// ---------------------------------------------------------------------------

void FxaaPass::create_render_pass(const vk::raii::Device& device, vk::Format surface_format) {
    vk::AttachmentDescription color_attachment{
        .format = surface_format,
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

    render_pass_ = vk::raii::RenderPass{device, create_info};
    spdlog::info("FXAA render pass created");
}

// ---------------------------------------------------------------------------
// FXAA framebuffers (per swapchain image)
// ---------------------------------------------------------------------------

void FxaaPass::create_framebuffers(const vk::raii::Device& device,
                                   vk::Extent2D extent,
                                   const std::vector<vk::raii::ImageView>& swapchain_image_views) {
    framebuffers_.clear();
    for (const auto& image_view : swapchain_image_views) {
        vk::ImageView attachment = *image_view;

        vk::FramebufferCreateInfo create_info{
            .renderPass = *render_pass_,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        framebuffers_.emplace_back(device, create_info);
    }
}

// ---------------------------------------------------------------------------
// FXAA descriptors
// ---------------------------------------------------------------------------

void FxaaPass::create_descriptors(const vk::raii::Device& device,
                                  const vk::raii::ImageView& offscreen_image_view) {
    // Sampler
    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    sampler_ = vk::raii::Sampler{device, sampler_info};

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
    descriptor_set_layout_ = vk::raii::DescriptorSetLayout{device, layout_info};

    // Pipeline layout
    vk::PipelineLayoutCreateInfo pl_layout_info{
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptor_set_layout_,
    };
    pipeline_layout_ = vk::raii::PipelineLayout{device, pl_layout_info};

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
    descriptor_pool_ = vk::raii::DescriptorPool{device, pool_info};

    // Allocate descriptor set
    vk::DescriptorSetAllocateInfo alloc_info{
        .descriptorPool = *descriptor_pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*descriptor_set_layout_,
    };
    auto sets = device.allocateDescriptorSets(alloc_info);
    descriptor_set_ = std::move(sets[0]);

    // Write initial descriptor
    update_descriptor(device, offscreen_image_view);

    spdlog::info("FXAA descriptors created");
}

void FxaaPass::update_descriptor(const vk::raii::Device& device,
                                 const vk::raii::ImageView& offscreen_image_view) {
    vk::DescriptorImageInfo image_info{
        .sampler = *sampler_,
        .imageView = *offscreen_image_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::WriteDescriptorSet write{
        .dstSet = *descriptor_set_,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &image_info,
    };

    device.updateDescriptorSets(write, {});
}

// ---------------------------------------------------------------------------
// FXAA pipeline
// ---------------------------------------------------------------------------

void FxaaPass::create_pipeline(const vk::raii::Device& device) {
    // Create shader modules from embedded SPIR-V
    vk::ShaderModuleCreateInfo vert_info{
        .codeSize = shaders::fullscreen_vert.size() * sizeof(uint32_t),
        .pCode = shaders::fullscreen_vert.data(),
    };
    vk::raii::ShaderModule vert_module{device, vert_info};

    vk::ShaderModuleCreateInfo frag_info{
        .codeSize = shaders::fxaa_frag.size() * sizeof(uint32_t),
        .pCode = shaders::fxaa_frag.data(),
    };
    vk::raii::ShaderModule frag_module{device, frag_info};

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
        .layout = *pipeline_layout_,
        .renderPass = *render_pass_,
        .subpass = 0,
    };

    pipeline_ = vk::raii::Pipeline{device, nullptr, pipeline_info};
    spdlog::info("FXAA pipeline created");
}

} // namespace steel

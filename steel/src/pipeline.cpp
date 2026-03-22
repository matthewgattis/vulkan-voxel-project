#include <steel/pipeline.hpp>

#include <array>
#include <stdexcept>

namespace steel {

PipelineBuilder::PipelineBuilder(const vk::raii::Device& device,
                                 std::span<const uint32_t> vert_spirv,
                                 std::span<const uint32_t> frag_spirv)
    : device_(device) {
    vk::ShaderModuleCreateInfo vert_info{
        .flags    = {},
        .codeSize = vert_spirv.size_bytes(),
        .pCode    = vert_spirv.data(),
    };
    vert_module_ = vk::raii::ShaderModule{device_, vert_info};

    vk::ShaderModuleCreateInfo frag_info{
        .flags    = {},
        .codeSize = frag_spirv.size_bytes(),
        .pCode    = frag_spirv.data(),
    };
    frag_module_ = vk::raii::ShaderModule{device_, frag_info};
}

PipelineBuilder& PipelineBuilder::set_vertex_input(
    std::span<const vk::VertexInputBindingDescription>   bindings,
    std::span<const vk::VertexInputAttributeDescription> attributes) {
    bindings_.assign(bindings.begin(), bindings.end());
    attributes_.assign(attributes.begin(), attributes.end());
    return *this;
}

PipelineBuilder& PipelineBuilder::set_topology(vk::PrimitiveTopology topology) {
    topology_ = topology;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_polygon_mode(vk::PolygonMode mode) {
    polygon_mode_ = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_cull_mode(vk::CullModeFlags cull, vk::FrontFace front) {
    cull_mode_  = cull;
    front_face_ = front;
    return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_test(bool enable, vk::CompareOp op) {
    depth_test_ = enable;
    depth_op_   = op;
    return *this;
}

vk::raii::Pipeline PipelineBuilder::build(
    const vk::raii::RenderPass&     render_pass,
    const vk::raii::PipelineLayout& layout) {

    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {{
        {.flags = {}, .stage = vk::ShaderStageFlagBits::eVertex,   .module = *vert_module_, .pName = "main"},
        {.flags = {}, .stage = vk::ShaderStageFlagBits::eFragment, .module = *frag_module_, .pName = "main"},
    }};

    vk::PipelineVertexInputStateCreateInfo vertex_input_info{
        .flags                           = {},
        .vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings_.size()),
        .pVertexBindingDescriptions      = bindings_.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes_.size()),
        .pVertexAttributeDescriptions    = attributes_.data(),
    };

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .flags                  = {},
        .topology               = topology_,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Dynamic viewport and scissor — set at draw time via begin_frame()
    std::array<vk::DynamicState, 2> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state{
        .flags             = {},
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data(),
    };

    vk::PipelineViewportStateCreateInfo viewport_state{
        .flags         = {},
        .viewportCount = 1,
        .pViewports    = nullptr,
        .scissorCount  = 1,
        .pScissors     = nullptr,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .flags                   = {},
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = polygon_mode_,
        .cullMode                = cull_mode_,
        .frontFace               = front_face_,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f,
        .lineWidth               = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .flags                = {},
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable  = VK_FALSE,
    };

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        .flags                 = {},
        .depthTestEnable       = depth_test_ ? VK_TRUE : VK_FALSE,
        .depthWriteEnable      = depth_test_ ? VK_TRUE : VK_FALSE,
        .depthCompareOp        = depth_op_,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable         = VK_FALSE,
        .srcColorBlendFactor = vk::BlendFactor::eOne,
        .dstColorBlendFactor = vk::BlendFactor::eZero,
        .colorBlendOp        = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp        = vk::BlendOp::eAdd,
        .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo color_blending{
        .flags           = {},
        .logicOpEnable   = VK_FALSE,
        .logicOp         = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments    = &color_blend_attachment,
        .blendConstants  = {{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .flags               = {},
        .stageCount          = static_cast<uint32_t>(shader_stages.size()),
        .pStages             = shader_stages.data(),
        .pVertexInputState   = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depth_stencil,
        .pColorBlendState    = &color_blending,
        .pDynamicState       = &dynamic_state,
        .layout              = *layout,
        .renderPass          = *render_pass,
        .subpass             = 0,
    };

    return vk::raii::Pipeline{device_, nullptr, pipeline_info};
}

} // namespace steel

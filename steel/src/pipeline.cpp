#include <steel/pipeline.hpp>

#include <array>
#include <stdexcept>

namespace steel {

PipelineBuilder::PipelineBuilder(const vk::raii::Device& device,
                                 std::span<const uint32_t> vert_spirv,
                                 std::span<const uint32_t> frag_spirv)
    : device_(device) {
    vk::ShaderModuleCreateInfo vert_info{
        {},
        vert_spirv.size_bytes(),
        vert_spirv.data(),
    };
    vert_module_ = vk::raii::ShaderModule{device_, vert_info};

    vk::ShaderModuleCreateInfo frag_info{
        {},
        frag_spirv.size_bytes(),
        frag_spirv.data(),
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
    const vk::raii::PipelineLayout& layout,
    vk::Extent2D                    extent) {

    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {{
        {{}, vk::ShaderStageFlagBits::eVertex,   *vert_module_, "main"},
        {{}, vk::ShaderStageFlagBits::eFragment, *frag_module_, "main"},
    }};

    vk::PipelineVertexInputStateCreateInfo vertex_input_info{
        {},
        bindings_,
        attributes_,
    };

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        {},
        topology_,
        VK_FALSE,
    };

    vk::Viewport viewport{
        0.0f, 0.0f,
        static_cast<float>(extent.width),
        static_cast<float>(extent.height),
        0.0f, 1.0f,
    };

    vk::Rect2D scissor{{0, 0}, extent};

    vk::PipelineViewportStateCreateInfo viewport_state{
        {},
        1, &viewport,
        1, &scissor,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        {},
        VK_FALSE,           // depthClampEnable
        VK_FALSE,           // rasterizerDiscardEnable
        polygon_mode_,
        cull_mode_,
        front_face_,
        VK_FALSE,           // depthBiasEnable
        0.0f, 0.0f, 0.0f,  // depthBias*
        1.0f,               // lineWidth
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        {},
        vk::SampleCountFlagBits::e1,
        VK_FALSE,
    };

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        {},
        depth_test_ ? VK_TRUE : VK_FALSE,  // depthTestEnable
        depth_test_ ? VK_TRUE : VK_FALSE,  // depthWriteEnable
        depth_op_,
        VK_FALSE,   // depthBoundsTestEnable
        VK_FALSE,   // stencilTestEnable
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment{
        VK_FALSE,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo color_blending{
        {},
        VK_FALSE,
        vk::LogicOp::eCopy,
        1, &color_blend_attachment,
        {{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    vk::GraphicsPipelineCreateInfo pipeline_info{
        {},
        shader_stages,
        &vertex_input_info,
        &input_assembly,
        nullptr,            // tessellation
        &viewport_state,
        &rasterizer,
        &multisampling,
        &depth_stencil,
        &color_blending,
        nullptr,            // dynamic state
        *layout,
        *render_pass,
        0,
    };

    return vk::raii::Pipeline{device_, nullptr, pipeline_info};
}

} // namespace steel

#include <glass/material.hpp>
#include <glass/vertex.hpp>
#include <steel/pipeline.hpp>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace glass {

Material::Material(vk::raii::PipelineLayout layout, vk::raii::Pipeline pipeline)
    : layout_(std::move(layout))
    , pipeline_(std::move(pipeline)) {}

void Material::bind(const vk::raii::CommandBuffer& cmd) const {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
}

Material Material::create(steel::Engine& engine,
                           const Shader& vertex_shader,
                           const Shader& fragment_shader,
                           const vk::raii::DescriptorSetLayout& frame_descriptor_layout) {
    auto binding = Vertex::binding_description();
    auto attributes = Vertex::attribute_descriptions();

    vk::PushConstantRange push_constant_range(
        vk::ShaderStageFlagBits::eVertex,
        0,
        sizeof(glm::mat4)
    );

    vk::DescriptorSetLayout set_layout = *frame_descriptor_layout;
    vk::PipelineLayoutCreateInfo layout_info(
        {},
        1, &set_layout,
        1, &push_constant_range
    );
    vk::raii::PipelineLayout layout(engine.device(), layout_info);

    auto pipeline = steel::PipelineBuilder(engine.device(),
                                            vertex_shader.spirv(),
                                            fragment_shader.spirv())
        .set_vertex_input(
            std::span<const vk::VertexInputBindingDescription>(&binding, 1),
            std::span<const vk::VertexInputAttributeDescription>(attributes))
        .set_topology(vk::PrimitiveTopology::eTriangleList)
        .set_cull_mode(vk::CullModeFlagBits::eBack)
        .set_depth_test(true, vk::CompareOp::eLess)
        .build(engine.render_pass(), layout);

    spdlog::info("Material created: pipeline and layout built");

    return Material(std::move(layout), std::move(pipeline));
}

} // namespace glass

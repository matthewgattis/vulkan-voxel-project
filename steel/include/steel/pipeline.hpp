#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <span>
#include <vector>

namespace steel {

class PipelineBuilder {
public:
    PipelineBuilder(const vk::raii::Device& device,
                    std::span<const uint32_t> vert_spirv,
                    std::span<const uint32_t> frag_spirv);

    // Vertex input
    PipelineBuilder& set_vertex_input(
        std::span<const vk::VertexInputBindingDescription>   bindings,
        std::span<const vk::VertexInputAttributeDescription> attributes);

    // Topology (default: TriangleList)
    PipelineBuilder& set_topology(vk::PrimitiveTopology topology);

    // Polygon mode (default: Fill)
    PipelineBuilder& set_polygon_mode(vk::PolygonMode mode);

    // Cull mode (default: Back, clockwise front face)
    PipelineBuilder& set_cull_mode(vk::CullModeFlags cull, vk::FrontFace front = vk::FrontFace::eClockwise);

    // Depth testing (default: enabled, Less)
    PipelineBuilder& set_depth_test(bool enable, vk::CompareOp op = vk::CompareOp::eLess);

    // Build the pipeline (viewport/scissor are dynamic)
    vk::raii::Pipeline build(
        const vk::raii::RenderPass&     render_pass,
        const vk::raii::PipelineLayout& layout);

private:
    const vk::raii::Device& device_;

    vk::raii::ShaderModule vert_module_ {nullptr};
    vk::raii::ShaderModule frag_module_ {nullptr};

    std::vector<vk::VertexInputBindingDescription>   bindings_;
    std::vector<vk::VertexInputAttributeDescription> attributes_;

    vk::PrimitiveTopology topology_     = vk::PrimitiveTopology::eTriangleList;
    vk::PolygonMode       polygon_mode_ = vk::PolygonMode::eFill;
    vk::CullModeFlags     cull_mode_    = vk::CullModeFlagBits::eBack;
    vk::FrontFace         front_face_   = vk::FrontFace::eClockwise;
    bool                  depth_test_   = true;
    vk::CompareOp         depth_op_     = vk::CompareOp::eLess;
};

} // namespace steel

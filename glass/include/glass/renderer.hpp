#pragma once

#include <glass/components.hpp>
#include <glass/scene_node.hpp>
#include <glass/world.hpp>
#include <steel/buffer.hpp>
#include <steel/engine.hpp>

#include <array>
#include <vector>

#include <glm/mat4x4.hpp>

namespace glass {

struct FrameUBO {
    glm::mat4 view_projection;
};

class Renderer {
public:
    explicit Renderer(steel::Engine& engine);

    void run(const SceneNode& root, const glm::mat4& view_projection);
    void render_frame(const SceneNode& root, const glm::mat4& view_projection);

    void run(World& world);
    void render_frame(World& world);

    const vk::raii::DescriptorSetLayout& frame_descriptor_layout() const { return frame_descriptor_layout_; }

private:
    void traverse(const vk::raii::CommandBuffer& cmd,
                  const SceneNode& node,
                  const glm::mat4& view_projection) const;

    void render_ecs(const vk::raii::CommandBuffer& cmd,
                    World& world,
                    uint32_t frame_index) const;

    void create_frame_descriptors();
    void update_frame_ubo(uint32_t frame_index, const glm::mat4& view_projection);

    steel::Engine& engine_;

    vk::raii::DescriptorSetLayout frame_descriptor_layout_{nullptr};
    vk::raii::DescriptorPool      frame_descriptor_pool_{nullptr};
    std::vector<vk::raii::DescriptorSet> frame_descriptor_sets_;
    std::vector<steel::Buffer>           frame_ubo_buffers_;
    std::vector<void*>                   frame_ubo_mapped_;
};

} // namespace glass

#pragma once

#include <glass/components.hpp>
#include <glass/world.hpp>
#include <steel/engine.hpp>
#include <steel/uniform_buffer.hpp>

#include <glm/mat4x4.hpp>

namespace glass {

struct FrameUBO {
    glm::mat4 view_projection;
};

class Renderer {
public:
    explicit Renderer(steel::Engine& engine);

    void bind_world(World& world);

    void set_camera(Entity camera) { camera_ = camera; }
    Entity camera() const { return camera_; }

    void run(World& world);
    void render_frame(World& world);

    const vk::raii::DescriptorSetLayout& frame_descriptor_layout() const { return frame_ubo_.layout(); }

private:
    void render_ecs(const vk::raii::CommandBuffer& cmd,
                    World& world,
                    uint32_t frame_index) const;

    steel::Engine& engine_;
    steel::UniformBuffer<FrameUBO> frame_ubo_;
    Entity camera_{null_entity};
};

} // namespace glass

#pragma once

#include <glass/camera.hpp>
#include <glass/components.hpp>
#include <glass/scene_node.hpp>
#include <glass/world.hpp>
#include <steel/engine.hpp>

namespace glass {

class Renderer {
public:
    explicit Renderer(steel::Engine& engine);

    void run(const SceneNode& root, Camera& camera);
    void render_frame(const SceneNode& root, Camera& camera);

    void run(Camera& camera, World& world);
    void render_frame(Camera& camera, World& world);

private:
    void traverse(const vk::raii::CommandBuffer& cmd,
                  const SceneNode& node,
                  const glm::mat4& view_projection) const;

    void render_ecs(const vk::raii::CommandBuffer& cmd,
                    const glm::mat4& view_projection,
                    World& world) const;

    steel::Engine& engine_;
};

} // namespace glass

#pragma once

#include <glass/camera.hpp>
#include <glass/scene_node.hpp>
#include <steel/engine.hpp>

namespace glass {

class Renderer {
public:
    explicit Renderer(steel::Engine& engine);

    void run(const SceneNode& root, Camera& camera);
    void render_frame(const SceneNode& root, Camera& camera);

private:
    void traverse(const vk::raii::CommandBuffer& cmd,
                  const SceneNode& node,
                  const glm::mat4& view_projection) const;

    steel::Engine& engine_;
};

} // namespace glass

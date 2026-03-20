#pragma once

#include <glass/scene_node.hpp>
#include <steel/engine.hpp>

namespace glass {

class Renderer {
public:
    explicit Renderer(steel::Engine& engine);

    void run(const SceneNode& root);
    void render_frame(const SceneNode& root);

private:
    void traverse(const vk::raii::CommandBuffer& cmd,
                  const SceneNode& node) const;

    steel::Engine& engine_;
};

} // namespace glass

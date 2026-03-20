#include <glass/renderer.hpp>

namespace glass {

Renderer::Renderer(steel::Engine& engine)
    : engine_(engine) {}

void Renderer::run(const SceneNode& root) {
    while (engine_.poll_events()) {
        render_frame(root);
    }
    engine_.wait_idle();
}

void Renderer::render_frame(const SceneNode& root) {
    auto* cmd = engine_.begin_frame();
    if (cmd) {
        traverse(*cmd, root);
        engine_.end_frame();
    }
}

void Renderer::traverse(const vk::raii::CommandBuffer& cmd,
                        const SceneNode& node) const {
    if (node.renderable) {
        node.renderable->material().bind(cmd);
        node.renderable->geometry().bind(cmd);
        node.renderable->geometry().draw(cmd);
    }

    for (const auto& child : node.children) {
        traverse(cmd, child);
    }
}

} // namespace glass

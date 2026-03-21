#include <glass/renderer.hpp>

#include <glm/glm.hpp>

namespace glass {

Renderer::Renderer(steel::Engine& engine)
    : engine_(engine) {}

void Renderer::run(const SceneNode& root, Camera& camera) {
    while (engine_.poll_events()) {
        render_frame(root, camera);
    }
    engine_.wait_idle();
}

void Renderer::render_frame(const SceneNode& root, Camera& camera) {
    auto extent = engine_.extent();
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    camera.set_aspect_ratio(aspect);

    auto* cmd = engine_.begin_frame();
    if (cmd) {
        traverse(*cmd, root, camera.view_projection());
        engine_.end_frame();
    }
}

void Renderer::run(Camera& camera, World& world) {
    while (engine_.poll_events()) {
        render_frame(camera, world);
    }
    engine_.wait_idle();
}

void Renderer::render_frame(Camera& camera, World& world) {
    auto extent = engine_.extent();
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    camera.set_aspect_ratio(aspect);

    auto* cmd = engine_.begin_frame();
    if (cmd) {
        render_ecs(*cmd, camera.view_projection(), world);
        engine_.end_frame();
    }
}

void Renderer::traverse(const vk::raii::CommandBuffer& cmd,
                        const SceneNode& node,
                        const glm::mat4& view_projection) const {
    if (node.renderable) {
        glm::mat4 mvp = view_projection * node.transform;

        node.renderable->material().bind(cmd);
        cmd.pushConstants<glm::mat4>(
            *node.renderable->material().layout(),
            vk::ShaderStageFlagBits::eVertex,
            0,
            mvp);
        node.renderable->geometry().bind(cmd);
        node.renderable->geometry().draw(cmd);
    }

    for (const auto& child : node.children) {
        traverse(cmd, child, view_projection);
    }
}

void Renderer::render_ecs(const vk::raii::CommandBuffer& cmd,
                          const glm::mat4& view_projection,
                          World& world) const {
    world.view<Transform, MeshComponent, MaterialComponent>()
        .each([&](Entity e, Transform& t, MeshComponent& mesh, MaterialComponent& mat) {
            glm::mat4 mvp = view_projection * t.matrix;
            mat.material->bind(cmd);
            cmd.pushConstants<glm::mat4>(
                *mat.material->layout(),
                vk::ShaderStageFlagBits::eVertex,
                0,
                mvp);
            mesh.geometry->bind(cmd);
            mesh.geometry->draw(cmd);
        });
}

} // namespace glass

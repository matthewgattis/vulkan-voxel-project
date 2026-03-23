#include <glass/renderer.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace glass {

Renderer::Renderer(steel::Engine& engine)
    : engine_(engine)
    , frame_ubo_{steel::UniformBuffer<FrameUBO>::create(
          engine_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)} {}

void Renderer::bind_world(World& world) {
    world.set_on_destroy([this](World& w, Entity e) {
        if (w.has<GeometryComponent>(e)) {
            auto& mesh = w.get<GeometryComponent>(e);
            if (mesh.geometry) {
                engine_.defer_destroy(std::move(mesh.geometry));
            }
        }
    });
}

void Renderer::run(World& world) {
    while (engine_.poll_events()) {
        render_frame(world);
    }
    engine_.wait_idle();
}

void Renderer::render_frame(World& world) {
    if (camera_ == null_entity || !world.alive(camera_)) {
        return;
    }
    if (!world.has<Transform>(camera_) || !world.has<CameraComponent>(camera_)) {
        return;
    }

    auto& t = world.get<Transform>(camera_);
    auto& cc = world.get<CameraComponent>(camera_);

    auto extent = engine_.extent();
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    cc.camera.set_aspect_ratio(aspect);

    glm::mat4 view = glm::inverse(t.matrix);
    glm::mat4 view_projection = cc.camera.projection() * view;

    auto* cmd = engine_.begin_frame();
    if (cmd) {
        frame_ubo_.update(engine_.current_frame(), FrameUBO{view_projection});
        render_ecs(*cmd, world, engine_.current_frame());
        engine_.end_frame();
    }
}

void Renderer::render_ecs(const vk::raii::CommandBuffer& cmd,
                          World& world,
                          uint32_t frame_index) const {
    world.view<Transform, GeometryComponent, MaterialComponent>()
        .each([&](Entity e, Transform& t, GeometryComponent& mesh, MaterialComponent& mat) {
            mat.material->bind(cmd);
            frame_ubo_.bind(cmd, mat.material->layout(), 0, frame_index);
            cmd.pushConstants<glm::mat4>(
                *mat.material->layout(),
                vk::ShaderStageFlagBits::eVertex,
                0,
                t.matrix);
            mesh.geometry->bind(cmd);
            mesh.geometry->draw(cmd);
        });
}

} // namespace glass

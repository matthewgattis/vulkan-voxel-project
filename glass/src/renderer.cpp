#include <glass/renderer.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace glass {

Renderer::Renderer(steel::Engine& engine)
    : engine_(engine)
    , frame_ubo_{steel::UniformBuffer<FrameUBO>::create(
          engine_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)}
    , xr_eye_ubos_{
          steel::UniformBuffer<FrameUBO>::create(
              engine_, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment),
          steel::UniformBuffer<FrameUBO>::create(
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
        frame_ubo_.update(engine_.current_frame(), FrameUBO{view, cc.camera.projection()});
        render_ecs(*cmd, world, engine_.current_frame(), frame_ubo_);
        engine_.end_frame();
    }
}

void Renderer::render_xr_eyes(const vk::raii::CommandBuffer& cmd,
                              World& world,
                              uint32_t frame_index,
                              steel::XrFrameState& frame_state,
                              steel::XrSystem& xr) {
    for (uint32_t eye = 0; eye < 2; ++eye) {
        xr_eye_ubos_[eye].update(frame_index,
                                 FrameUBO{frame_state.eyes[eye].view,
                                          frame_state.eyes[eye].projection});

        xr.begin_eye_render(cmd, eye);
        render_ecs(cmd, world, frame_index, xr_eye_ubos_[eye]);
        xr.end_eye_render(cmd, eye);
    }
}

void Renderer::render_desktop_companion(const vk::raii::CommandBuffer& cmd,
                                        World& world,
                                        uint32_t frame_index,
                                        const glm::mat4* xr_view) {
    if (camera_ == null_entity || !world.alive(camera_)) return;
    if (!world.has<Transform>(camera_) || !world.has<CameraComponent>(camera_)) return;

    auto& cc = world.get<CameraComponent>(camera_);

    glm::mat4 view = xr_view ? *xr_view : glm::inverse(world.get<Transform>(camera_).matrix);
    frame_ubo_.update(frame_index, FrameUBO{view, cc.camera.projection()});
    render_ecs(cmd, world, frame_index, frame_ubo_);
}

void Renderer::render_ecs(const vk::raii::CommandBuffer& cmd,
                          World& world,
                          uint32_t frame_index,
                          const steel::UniformBuffer<FrameUBO>& ubo) const {
    world.view<Transform, GeometryComponent, MaterialComponent>()
        .each([&](Entity e, Transform& t, GeometryComponent& mesh, MaterialComponent& mat) {
            mat.material->bind(cmd);
            ubo.bind(cmd, mat.material->layout(), 0, frame_index);
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

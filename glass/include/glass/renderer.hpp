#pragma once

#include <glass/components.hpp>
#include <glass/world.hpp>
#include <steel/engine.hpp>
#include <steel/uniform_buffer.hpp>
#include <steel/xr_system.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>

namespace glass {

struct FrameUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

class Renderer {
public:
    explicit Renderer(steel::Engine& engine);

    void bind_world(World& world);

    void set_camera(Entity camera) { camera_ = camera; }
    Entity camera() const { return camera_; }

    void run(World& world);
    void render_frame(World& world);

    // Render both eyes into XR swapchains. Call between
    // engine.begin_command_buffer() and engine.end_frame().
    void render_xr_eyes(const vk::raii::CommandBuffer& cmd,
                        World& world,
                        uint32_t frame_index,
                        steel::XrFrameState& frame_state,
                        steel::XrSystem& xr);

    // Render desktop companion view. Called when begin_scene_pass() has
    // already started the offscreen render pass (XR dual-path mode).
    // When xr_view is provided, the mirror matches the headset instead of
    // using the CameraController's orientation.
    void render_desktop_companion(const vk::raii::CommandBuffer& cmd,
                                  World& world,
                                  uint32_t frame_index,
                                  const glm::mat4* xr_view = nullptr);

    const vk::raii::DescriptorSetLayout& frame_descriptor_layout() const { return frame_ubo_.layout(); }

private:
    void render_ecs(const vk::raii::CommandBuffer& cmd,
                    World& world,
                    uint32_t frame_index,
                    const steel::UniformBuffer<FrameUBO>& ubo) const;

    steel::Engine& engine_;
    steel::UniformBuffer<FrameUBO> frame_ubo_;
    // Separate UBOs per XR eye so each eye has its own buffer.
    // Without these, both eyes and the desktop companion would write to the
    // same mapped buffer (frame_ubo_), and only the last memcpy would survive
    // by the time the GPU executes the recorded command buffer.
    std::array<steel::UniformBuffer<FrameUBO>, 2> xr_eye_ubos_;
    Entity camera_{null_entity};
};

} // namespace glass

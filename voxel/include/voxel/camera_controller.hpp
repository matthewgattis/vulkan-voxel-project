#pragma once

#include <glass/event_dispatcher.hpp>
#include <glass/world.hpp>

#include <glm/vec3.hpp>

#include <SDL3/SDL_scancode.h>

namespace voxel {

class CameraController {
public:
    CameraController(glass::EventDispatcher& dispatcher,
                     float yaw, float pitch);

    // move_forward: optional override for movement direction (XY plane, normalized).
    // When provided (XR mode), WASD moves relative to this direction instead of yaw_.
    void update(float dt, glass::World& world, glass::Entity camera_entity,
                const glm::vec3* move_forward = nullptr);

    float yaw() const { return yaw_; }

private:
    static constexpr float MOUSE_SENSITIVITY = 0.002f;
    static constexpr float FRICTION = 4.0f;
    static constexpr float MAX_SPEED = 22.0f;
    static constexpr float SPRINT_SPEED = 44.0f;
    static constexpr float MAX_PITCH = 1.553f; // ~89 degrees

    float yaw_;
    float pitch_;
    bool sprinting_ = false;

    // Input state (managed by event subscription)
    bool keys_[SDL_SCANCODE_COUNT] = {};
    float mouse_dx_ = 0.0f;
    float mouse_dy_ = 0.0f;

    glass::Subscription subscription_;
};

} // namespace voxel

#include <voxel/camera_controller.hpp>

#include <glass/components.hpp>
#include <steel/engine.hpp>

#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace voxel {

CameraController::CameraController(glm::vec3 position, float yaw, float pitch)
    : position_{position}
    , yaw_{yaw}
    , pitch_{pitch}
{
}

void CameraController::update(steel::Engine& engine, glass::World& world, glass::Entity camera_entity) {
    float dt = engine.delta_time();
    const bool* keys = engine.keyboard_state();

    // Mouse look: yaw around Z (world up), pitch around local X (right)
    yaw_ -= engine.mouse_dx() * MOUSE_SENSITIVITY;
    pitch_ -= engine.mouse_dy() * MOUSE_SENSITIVITY;
    pitch_ = std::clamp(pitch_, -MAX_PITCH, MAX_PITCH);

    // Base rotation: camera looks along local -Z with +Y up (OpenGL convention).
    // Rotate 90° around X so local -Z maps to world +Y (forward) and local +Y maps to world +Z (up).
    static const glm::mat4 base_rot = glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});

    // Yaw around world Z, pitch around local X, then apply base orientation
    glm::mat4 rot = glm::rotate(glm::mat4{1.0f}, yaw_, glm::vec3{0.0f, 0.0f, 1.0f})
                  * glm::rotate(glm::mat4{1.0f}, pitch_, glm::vec3{1.0f, 0.0f, 0.0f})
                  * base_rot;

    // Movement vectors projected onto XY plane (Z-up)
    glm::vec3 flat_forward = glm::normalize(glm::vec3{-std::sin(yaw_), std::cos(yaw_), 0.0f});
    glm::vec3 flat_right   = glm::normalize(glm::vec3{ std::cos(yaw_), std::sin(yaw_), 0.0f});
    glm::vec3 up           = glm::vec3{0.0f, 0.0f, 1.0f};

    // Accumulate input as acceleration
    auto& vel = world.get<glass::Velocity>(camera_entity);
    glm::vec3 accel{0.0f};
    if (keys[SDL_SCANCODE_W])      accel += flat_forward;
    if (keys[SDL_SCANCODE_S])      accel -= flat_forward;
    if (keys[SDL_SCANCODE_D])      accel += flat_right;
    if (keys[SDL_SCANCODE_A])      accel -= flat_right;
    if (keys[SDL_SCANCODE_SPACE])  accel += up;
    if (keys[SDL_SCANCODE_LSHIFT]) accel -= up;

    bool moving = glm::length(accel) > 0.0f;

    // Sprint: latch on when Ctrl pressed while moving, release when movement stops
    if (!moving) {
        sprinting_ = false;
    } else if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_TAB]) {
        sprinting_ = true;
    }


    // Use the appropriate max speed for acceleration and clamping
    float current_max = sprinting_ ? SPRINT_SPEED : MAX_SPEED;

    if (moving) {
        accel = glm::normalize(accel) * (current_max * FRICTION);
    }

    // Apply acceleration
    vel.linear += accel * dt;

    // Apply friction (subtractive, proportional to speed, can't reverse direction)
    float speed = glm::length(vel.linear);
    if (speed > 0.0f) {
        float friction_loss = FRICTION * speed * dt;
        friction_loss = std::min(friction_loss, speed);
        vel.linear -= glm::normalize(vel.linear) * friction_loss;
    }

    // Hard clamp speed
    speed = glm::length(vel.linear);
    if (speed > current_max) {
        vel.linear = glm::normalize(vel.linear) * current_max;
    }

    // Integrate position
    position_ += vel.linear * dt;

    // Build world-space camera transform: translate * rotate
    glm::mat4 transform = glm::translate(glm::mat4{1.0f}, position_) * rot;
    world.get<glass::Transform>(camera_entity).matrix = transform;
}

} // namespace voxel

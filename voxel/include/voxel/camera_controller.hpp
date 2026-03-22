#pragma once

#include <glass/world.hpp>

#include <glm/vec3.hpp>

namespace steel { class Engine; }

namespace voxel {

class CameraController {
public:
    CameraController(glm::vec3 position, float yaw, float pitch);

    void update(steel::Engine& engine, glass::World& world, glass::Entity camera_entity);

private:
    static constexpr float MOUSE_SENSITIVITY = 0.002f;
    static constexpr float FRICTION = 8.0f;
    static constexpr float MAX_SPEED = 11.0f;
    static constexpr float SPRINT_SPEED = 22.0f;
    static constexpr float MAX_PITCH = 1.553f; // ~89 degrees

    glm::vec3 position_;
    float yaw_;
    float pitch_;
    bool sprinting_ = false;
};

} // namespace voxel

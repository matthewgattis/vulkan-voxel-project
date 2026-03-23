#pragma once

#include <glass/camera.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>

#include <glm/mat4x4.hpp>

#include <memory>

namespace glass {

struct Transform {
    glm::mat4 matrix{1.0f};
};

struct GeometryComponent {
    std::unique_ptr<Geometry> geometry;
};

struct MaterialComponent {
    const Material* material{nullptr};
};

struct Velocity {
    glm::vec3 linear{0.0f};
};

struct CameraComponent {
    Camera camera;
};

} // namespace glass

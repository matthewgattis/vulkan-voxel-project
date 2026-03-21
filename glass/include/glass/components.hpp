#pragma once

#include <glass/geometry.hpp>
#include <glass/material.hpp>

#include <glm/mat4x4.hpp>

namespace glass {

struct Transform {
    glm::mat4 matrix{1.0f};
};

struct MeshComponent {
    const Geometry* geometry{nullptr};
};

struct MaterialComponent {
    const Material* material{nullptr};
};

} // namespace glass

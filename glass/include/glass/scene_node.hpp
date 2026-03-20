#pragma once

#include <glass/renderable.hpp>
#include <glm/mat4x4.hpp>
#include <vector>

namespace glass {

struct SceneNode {
    glm::mat4 transform{1.0f};
    const Renderable* renderable{nullptr};
    std::vector<SceneNode> children;
};

} // namespace glass

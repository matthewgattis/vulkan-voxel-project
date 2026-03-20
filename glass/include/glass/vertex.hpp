#pragma once

#include <array>
#include <glm/vec3.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace glass {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;

    static vk::VertexInputBindingDescription binding_description();
    static std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions();
};

} // namespace glass

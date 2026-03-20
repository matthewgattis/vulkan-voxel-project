#include <glass/vertex.hpp>
#include <cstddef>

namespace glass {

vk::VertexInputBindingDescription Vertex::binding_description() {
    return vk::VertexInputBindingDescription(
        0,                            // binding
        36,                           // stride (3 * sizeof(glm::vec3))
        vk::VertexInputRate::eVertex  // inputRate
    );
}

std::array<vk::VertexInputAttributeDescription, 3> Vertex::attribute_descriptions() {
    return {{
        vk::VertexInputAttributeDescription(
            0,                              // location
            0,                              // binding
            vk::Format::eR32G32B32Sfloat,   // format
            0                               // offset (position)
        ),
        vk::VertexInputAttributeDescription(
            1,                              // location
            0,                              // binding
            vk::Format::eR32G32B32Sfloat,   // format
            12                              // offset (normal)
        ),
        vk::VertexInputAttributeDescription(
            2,                              // location
            0,                              // binding
            vk::Format::eR32G32B32Sfloat,   // format
            24                              // offset (color)
        ),
    }};
}

} // namespace glass

#pragma once

#include <glass/vertex.hpp>
#include <cstdint>
#include <span>

namespace glass {

class Mesh {
public:
    virtual ~Mesh() = default;
    virtual std::span<const Vertex> vertices() const = 0;
    virtual std::span<const uint32_t> indices() const = 0;
};

} // namespace glass

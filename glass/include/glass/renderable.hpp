#pragma once

#include <glass/geometry.hpp>
#include <glass/material.hpp>

namespace glass {

class Renderable {
public:
    Renderable(Geometry geometry, Material material);

    const Geometry& geometry() const { return geometry_; }
    const Material& material() const { return material_; }

    Renderable(Renderable&&) = default;
    Renderable& operator=(Renderable&&) = default;

private:
    Geometry geometry_;
    Material material_;
};

} // namespace glass

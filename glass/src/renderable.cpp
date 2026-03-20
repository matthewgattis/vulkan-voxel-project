#include <glass/renderable.hpp>

#include <spdlog/spdlog.h>

namespace glass {

Renderable::Renderable(Geometry geometry, Material material)
    : geometry_(std::move(geometry))
    , material_(std::move(material)) {
    spdlog::info("Renderable created");
}

} // namespace glass

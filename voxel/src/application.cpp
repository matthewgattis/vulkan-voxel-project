#include <voxel/application.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>

namespace voxel {

Application::Application()
    : engine_{"Voxel", 1280, 720}
    , renderable_{
          glass::Geometry::create(engine_, TriangleMesh{}),
          glass::Material::create(
              engine_,
              glass::Shader::load(vk::ShaderStageFlagBits::eVertex,
                                  std::filesystem::path{SHADER_DIR} / "triangle.vert.spv"),
              glass::Shader::load(vk::ShaderStageFlagBits::eFragment,
                                  std::filesystem::path{SHADER_DIR} / "triangle.frag.spv"))}
    , renderer_{engine_}
{
    scene_.renderable = &renderable_;
    spdlog::info("Application initialized");
}

Application::~Application() {
    spdlog::info("Application shutting down");
}

void Application::run() {
    renderer_.run(scene_);
}

} // namespace voxel

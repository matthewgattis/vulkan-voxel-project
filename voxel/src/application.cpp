#include <voxel/application.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>

namespace voxel {

Application::Application()
    : engine_{"Voxel", 1280, 960}
    , renderable_{
          glass::Geometry::create(engine_, CubeMesh{}),
          glass::Material::create(
              engine_,
              glass::Shader::load(vk::ShaderStageFlagBits::eVertex,
                                  std::filesystem::path{SHADER_DIR} / "triangle.vert.spv"),
              glass::Shader::load(vk::ShaderStageFlagBits::eFragment,
                                  std::filesystem::path{SHADER_DIR} / "triangle.frag.spv"))}
    , camera_{60.0f,
              static_cast<float>(engine_.extent().width) / static_cast<float>(engine_.extent().height),
              0.1f, 100.0f}
    , renderer_{engine_}
{
    camera_.set_position({2.0f, 2.0f, 2.0f});
    camera_.look_at({0.0f, 0.0f, 0.0f});

    scene_.renderable = &renderable_;
    spdlog::info("Application initialized");
}

Application::~Application() {
    spdlog::info("Application shutting down");
}

void Application::run() {
    renderer_.run(scene_, camera_);
}

} // namespace voxel

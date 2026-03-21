#include <voxel/application.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>

namespace voxel {

Application::Application()
    : engine_{"Voxel", 1280, 960}
    , geometry_{glass::Geometry::create(engine_, CubeMesh{})}
    , material_{glass::Material::create(
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

    auto cube = world_.create();
    world_.add<glass::Transform>(cube, glass::Transform{glm::mat4{1.0f}});
    world_.add<glass::MeshComponent>(cube, glass::MeshComponent{&geometry_});
    world_.add<glass::MaterialComponent>(cube, glass::MaterialComponent{&material_});

    spdlog::info("Application initialized");
}

Application::~Application() {
    spdlog::info("Application shutting down");
}

void Application::run() {
    renderer_.run(camera_, world_);
}

} // namespace voxel

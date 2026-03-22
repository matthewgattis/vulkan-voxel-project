#include <voxel/application.hpp>

#include <spdlog/spdlog.h>

#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>

namespace voxel {

Application::Application()
    : engine_{"Voxel", 1280, 960}
    , geometry_{glass::Geometry::create(engine_, CubeMesh{})}
    , renderer_{engine_}
    , material_{glass::Material::create(
          engine_,
          glass::Shader::load(vk::ShaderStageFlagBits::eVertex,
                              std::filesystem::path{SHADER_DIR} / "triangle.vert.spv"),
          glass::Shader::load(vk::ShaderStageFlagBits::eFragment,
                              std::filesystem::path{SHADER_DIR} / "triangle.frag.spv"),
          renderer_.frame_descriptor_layout())}
{
    auto cam = world_.create();
    glass::Camera camera{60.0f, static_cast<float>(engine_.extent().width) / static_cast<float>(engine_.extent().height), 0.1f, 100.0f};
    // Camera transform is the inverse of the view matrix (world-space pose)
    glm::mat4 cam_transform = glm::inverse(glm::lookAt(
        glm::vec3{2.0f, 2.0f, 2.0f},  // position
        glm::vec3{0.0f, 0.0f, 0.0f},  // target
        glm::vec3{0.0f, 1.0f, 0.0f}   // up
    ));
    world_.add<glass::Transform>(cam, glass::Transform{cam_transform});
    world_.add<glass::CameraComponent>(cam, glass::CameraComponent{std::move(camera)});
    renderer_.set_camera(cam);

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
    renderer_.run(world_);
}

} // namespace voxel

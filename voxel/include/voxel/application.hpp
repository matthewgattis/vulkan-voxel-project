#pragma once

#include <steel/engine.hpp>
#include <glass/shader.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/renderer.hpp>
#include <glass/world.hpp>
#include <glass/components.hpp>
#include <glass/mesh.hpp>
#include <glass/vertex.hpp>

#include <voxel/camera_controller.hpp>
#include <voxel/terrain.hpp>

namespace voxel {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    auto operator=(const Application&) -> Application& = delete;
    Application(Application&&) = delete;
    auto operator=(Application&&) -> Application& = delete;

    void run();

private:
    steel::Engine engine_;
    glass::Renderer renderer_;
    glass::Material material_;
    glass::World world_;
    Terrain terrain_;
    glass::Entity camera_entity_;
    CameraController camera_controller_;

    // FPS display (updated once per second)
    float fps_display_ = 0.0f;
    float fps_ms_display_ = 0.0f;
    float fps_timer_ = 0.0f;
    int fps_frame_count_ = 0;
};

} // namespace voxel

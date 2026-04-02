#pragma once

#include <steel/engine.hpp>
#include <glass/event_dispatcher.hpp>
#include <glass/shader.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/renderer.hpp>
#include <glass/world.hpp>
#include <glass/components.hpp>
#include <glass/mesh.hpp>
#include <glass/vertex.hpp>

#include <voxel/camera_controller.hpp>
#include <voxel/chunk_manager.hpp>
#include <voxel/terrain_generator.hpp>

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
    glass::EventDispatcher event_dispatcher_;
    glass::Renderer renderer_;
    glass::Material material_;
    glass::World world_;
    TerrainGenerator terrain_generator_;
    ChunkManager chunk_manager_;
    glass::Entity camera_entity_;

    // Subscriptions and camera controller are ordered so that ImGui and
    // mouse capture subscribe before CameraController (member init order).
    glass::Subscription imgui_sub_;
    glass::Subscription mouse_capture_sub_;
    glass::Subscription key_sub_;
    CameraController camera_controller_;

    // Mouse capture state (app-level UX: click to enter look mode)
    float mouse_capture_x_ = 0.0f;
    float mouse_capture_y_ = 0.0f;
    bool mouse_captured_ = false;

    // FPS display (updated once per second)
    float fps_display_ = 0.0f;
    float fps_ms_display_ = 0.0f;
    float fps_timer_ = 0.0f;
    int fps_frame_count_ = 0;
};

} // namespace voxel

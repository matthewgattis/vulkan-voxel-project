#include <voxel/application.hpp>

#include <imgui.h>
#include <vk_mem_alloc.h>
#include <spdlog/spdlog.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>

namespace voxel {

Application::Application()
    : engine_{"Voxel"}
    , renderer_{engine_}
    , material_{glass::Material::create(
          engine_,
          glass::Shader::load(vk::ShaderStageFlagBits::eVertex,
                              std::filesystem::path{SHADER_DIR} / "triangle.vert.spv"),
          glass::Shader::load(vk::ShaderStageFlagBits::eFragment,
                              std::filesystem::path{SHADER_DIR} / "triangle.frag.spv"),
          renderer_.frame_descriptor_layout())}
    , chunk_manager_{engine_, world_, material_, terrain_generator_}
    , camera_entity_{world_.create()}
    , camera_controller_{
          glm::vec3{32.0f, -20.0f, 140.0f},
          0.0f,                                     // yaw: looking along +Y
          std::atan2(-30.0f, 52.0f)}                // pitch: direction (0,52,-30)
{
    glass::Camera camera{60.0f,
        static_cast<float>(engine_.extent().width) / static_cast<float>(engine_.extent().height),
        0.1f, 500.0f};

    world_.add<glass::Transform>(camera_entity_, glass::Transform{glm::mat4{1.0f}});
    world_.add<glass::Velocity>(camera_entity_, glass::Velocity{});
    world_.add<glass::CameraComponent>(camera_entity_, glass::CameraComponent{std::move(camera)});
    renderer_.set_camera(camera_entity_);
    renderer_.bind_world(world_);

    // Set up event callback
    engine_.set_event_callback([this](const SDL_Event& event) { handle_event(event); });

    // Set the initial camera transform via one controller update
    camera_controller_.update(0.0f, 0.0f, 0.0f, engine_.keyboard_state(), world_, camera_entity_);

    spdlog::info("Application initialized");
}

Application::~Application() {
    spdlog::info("Application shutting down");
}

void Application::handle_event(const SDL_Event& event) {
    // Forward to ImGui when mouse is not captured
    if (engine_.imgui_enabled() && !mouse_captured_) {
        engine_.imgui_process_event(event);
    }

    // Toggle ImGui with F3
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F3 &&
        !event.key.repeat) {
        engine_.set_imgui_enabled(!engine_.imgui_enabled());
    }

    // Mouse capture
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (!engine_.imgui_enabled() || !ImGui::GetIO().WantCaptureMouse) {
            mouse_captured_ = true;
            mouse_capture_first_frame_ = true;
            SDL_SetWindowRelativeMouseMode(engine_.window(), true);
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
        mouse_captured_ = false;
        SDL_SetWindowRelativeMouseMode(engine_.window(), false);
    }

    // Accumulate mouse motion
    if (event.type == SDL_EVENT_MOUSE_MOTION && mouse_captured_ && !mouse_capture_first_frame_) {
        mouse_dx_ += event.motion.xrel;
        mouse_dy_ += event.motion.yrel;
    }
}

void Application::run() {
    while (engine_.poll_events()) {
        // Update camera with accumulated input
        camera_controller_.update(engine_.delta_time(), mouse_dx_, mouse_dy_,
                                  engine_.keyboard_state(), world_, camera_entity_);

        // Reset per-frame mouse accumulators
        mouse_dx_ = 0.0f;
        mouse_dy_ = 0.0f;
        mouse_capture_first_frame_ = false;

        // Load/unload chunks around camera
        chunk_manager_.update(camera_controller_.position());

        // Update FPS counter once per second
        fps_timer_ += engine_.delta_time();
        fps_frame_count_++;
        if (fps_timer_ >= 1.0f) {
            fps_display_ = static_cast<float>(fps_frame_count_) / fps_timer_;
            fps_ms_display_ = fps_timer_ / static_cast<float>(fps_frame_count_) * 1000.0f;
            fps_frame_count_ = 0;
            fps_timer_ = 0.0f;
        }

        // ImGui frame
        engine_.imgui_begin();
        if (engine_.imgui_enabled()) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGui::Begin("Debug", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav);
            ImGui::Text("%.1f FPS (%.2f ms)", fps_display_, fps_ms_display_);

            ImGui::Separator();
            VmaTotalStatistics stats{};
            vmaCalculateStatistics(engine_.allocator(), &stats);
            const auto& total = stats.total;
            ImGui::Text("VMA: %u allocs, %u blocks",
                        total.statistics.allocationCount,
                        total.statistics.blockCount);
            ImGui::Text("  Used: %.2f MB / %.2f MB",
                        static_cast<double>(total.statistics.allocationBytes) / (1024.0 * 1024.0),
                        static_cast<double>(total.statistics.blockBytes) / (1024.0 * 1024.0));
            ImGui::End();
        }
        engine_.imgui_end();

        renderer_.render_frame(world_);
    }
    engine_.wait_idle();
}

} // namespace voxel

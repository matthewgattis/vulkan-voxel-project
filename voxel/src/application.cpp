#include <voxel/application.hpp>

#include <imgui.h>
#include <vk_mem_alloc.h>
#include <spdlog/spdlog.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>

namespace voxel {

Application::Application()
    : engine_{"Voxel", 1280, 960}
    , renderer_{engine_}
    , material_{glass::Material::create(
          engine_,
          glass::Shader::load(vk::ShaderStageFlagBits::eVertex,
                              std::filesystem::path{SHADER_DIR} / "triangle.vert.spv"),
          glass::Shader::load(vk::ShaderStageFlagBits::eFragment,
                              std::filesystem::path{SHADER_DIR} / "triangle.frag.spv"),
          renderer_.frame_descriptor_layout())}
    , terrain_{engine_, world_, material_}
    , camera_entity_{world_.create()}
    , camera_controller_{
          glm::vec3{32.0f, -20.0f, 40.0f},
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

    // Set the initial camera transform via one controller update
    camera_controller_.update(engine_, world_, camera_entity_);

    spdlog::info("Application initialized");
}

Application::~Application() {
    spdlog::info("Application shutting down");
}

void Application::run() {
    while (engine_.poll_events()) {
        camera_controller_.update(engine_, world_, camera_entity_);

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

#include <voxel/application.hpp>

#include <imgui.h>
#include <vk_mem_alloc.h>
#include <spdlog/spdlog.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>

namespace voxel {

// Query XR requirements and build the engine config. Called before Engine
// construction so XR-required Vulkan extensions can be added to the instance.
steel::EngineConfig Application::build_engine_config() {
    steel::EngineConfig config{.title = "Voxel"};

    auto xr_reqs = steel::XrSystem::query_requirements();
    if (xr_reqs) {
        config.extra_instance_extensions = std::move(xr_reqs->instance_extensions);
        config.extra_device_extensions = std::move(xr_reqs->device_extensions);
        // Don't cap Vulkan API version from XR runtime's maxApiVersionSupported.
        // With XR_KHR_vulkan_enable (v1) the app creates its own VkInstance/VkDevice,
        // so the runtime's reported max is advisory, not enforced. The engine needs
        // its native API version (1.3) for correct rendering.
        config.physical_device_query = steel::XrSystem::query_physical_device;
    }

    return config;
}

Application::Application()
    : engine_{build_engine_config()}
    , event_dispatcher_{engine_}
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
    // Subscribe event handlers (order matters: ImGui and capture before CameraController)
    , imgui_sub_{event_dispatcher_.subscribe([this](const SDL_Event& event, bool& handled) {
        // Always forward to ImGui so its internal state (including
        // SDL_CaptureMouse) stays consistent across down/up pairs.
        engine_.imgui_process_event(event);
        if (engine_.imgui_enabled()) {
            if (ImGui::GetIO().WantCaptureMouse && !mouse_captured_) {
                handled = true;
            }
        }
    })}
    , mouse_capture_sub_{event_dispatcher_.subscribe([this](const SDL_Event& event, bool& handled) {
        if (handled) return;

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            int w, h;
            SDL_GetWindowSize(engine_.window(), &w, &h);
            bool in_client_area = event.button.x >= 0 && event.button.x < w &&
                                  event.button.y >= 0 && event.button.y < h;
            if (in_client_area) {
                SDL_GetMouseState(&mouse_capture_x_, &mouse_capture_y_);
                mouse_captured_ = true;
                SDL_SetWindowRelativeMouseMode(engine_.window(), true);
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
            mouse_captured_ = false;
            SDL_WarpMouseInWindow(engine_.window(), mouse_capture_x_, mouse_capture_y_);
            SDL_SetWindowRelativeMouseMode(engine_.window(), false);
        }

        // Block mouse motion from reaching other subscribers when not captured
        if (event.type == SDL_EVENT_MOUSE_MOTION && !mouse_captured_) {
            handled = true;
        }
    })}
    , key_sub_{event_dispatcher_.subscribe([this](const SDL_Event& event, bool&) {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F3 &&
            !event.key.repeat) {
            engine_.set_imgui_enabled(!engine_.imgui_enabled());
        }
    })}
    , camera_controller_{
          event_dispatcher_,
          0.0f,                                     // yaw: looking along +Y
          std::atan2(-30.0f, 52.0f)}                // pitch: direction (0,52,-30)
{
    glass::Camera camera{60.0f,
        static_cast<float>(engine_.extent().width) / static_cast<float>(engine_.extent().height),
        0.1f, 500.0f};

    glm::vec3 start_pos{32.0f, -20.0f, 80.0f};
    world_.add<glass::Transform>(camera_entity_, glass::Transform{glm::translate(glm::mat4{1.0f}, start_pos)});
    world_.add<glass::Velocity>(camera_entity_, glass::Velocity{});
    world_.add<glass::CameraComponent>(camera_entity_, glass::CameraComponent{std::move(camera)});
    renderer_.set_camera(camera_entity_);
    renderer_.bind_world(world_);

    // Set the initial camera orientation via one controller update
    camera_controller_.update(0.0f, world_, camera_entity_);

    // Initialize OpenXR if query_requirements() found an HMD
    // (the static XR instance was created during build_engine_config)
    if (steel::XrSystem::has_pending_session()) {
        xr_system_ = std::make_unique<steel::XrSystem>(
            static_cast<VkInstance>(*engine_.instance()),
            static_cast<VkPhysicalDevice>(*engine_.physical_device()),
            static_cast<VkDevice>(*engine_.device()),
            engine_.graphics_family(), 0,
            engine_.allocator(),
            engine_.color_format(), engine_.depth_format(),
            engine_.device());
    }

    spdlog::info("Application initialized");
}

Application::~Application() {
    spdlog::info("Application shutting down");
    engine_.wait_idle();
    // Destroy XR system before engine (XR owns Vulkan resources)
    xr_system_.reset();
}

void Application::run() {
    while (engine_.poll_events()) {
        if (xr_system_) xr_system_->poll_events();

        // Update camera with accumulated input.
        // In XR mode, movement follows the headset direction (from previous frame).
        if (xr_system_ && xr_system_->active()) {
            camera_controller_.update(engine_.delta_time(), world_, camera_entity_,
                                      &xr_move_forward_);
        } else {
            camera_controller_.update(engine_.delta_time(), world_, camera_entity_);
        }

        // Compute view-projection for frustum culling
        auto& cam_transform = world_.get<glass::Transform>(camera_entity_);
        auto& cam_component = world_.get<glass::CameraComponent>(camera_entity_);
        glm::mat4 view = glm::inverse(cam_transform.matrix);
        glm::mat4 vp = cam_component.camera.projection() * view;

        // Load/unload chunks around camera
        glm::vec3 camera_pos = glm::vec3(cam_transform.matrix[3]);
        chunk_manager_.update(camera_pos, vp);

        // Update FPS counter once per second
        fps_timer_ += engine_.delta_time();
        fps_frame_count_++;
        if (fps_timer_ >= 1.0f) {
            fps_display_ = static_cast<float>(fps_frame_count_) / fps_timer_;
            fps_ms_display_ = fps_timer_ / static_cast<float>(fps_frame_count_) * 1000.0f;
            fps_frame_count_ = 0;
            fps_timer_ = 0.0f;
        }

        // ImGui frame (desktop companion only)
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

            if (xr_system_) {
                ImGui::Text("XR: %s", xr_system_->active() ? "active" : "inactive");
                if (xr_system_->active()) {
                    auto ext = xr_system_->eye_extent();
                    ImGui::Text("  Eye: %ux%u", ext.width, ext.height);
                }
            }

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

        // Render XR eyes + desktop companion, or desktop-only
        if (xr_system_ && xr_system_->active()) {
            auto xr_state = xr_system_->wait_and_begin_frame(
                camera_pos, camera_controller_.yaw());

            // Extract headset forward for next frame's movement direction
            if (xr_state.should_render) {
                glm::mat4 head_world = glm::inverse(xr_state.eyes[0].view);
                glm::vec3 forward_3d = -glm::vec3(head_world[2]); // -Z column = forward
                xr_move_forward_ = glm::vec3(forward_3d.x, forward_3d.y, 0.0f);
            }

            auto* cmd = engine_.begin_command_buffer();
            if (cmd) {
                // Render both eyes into XR swapchains
                if (xr_state.should_render) {
                    renderer_.render_xr_eyes(*cmd, world_, engine_.current_frame(),
                                             xr_state, *xr_system_);
                }

                // Desktop companion mirrors the headset (left eye view)
                engine_.begin_scene_pass();
                auto extent = engine_.extent();
                float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
                cam_component.camera.set_aspect_ratio(aspect);
                const glm::mat4* mirror_view =
                    xr_state.should_render ? &xr_state.eyes[0].view : nullptr;
                renderer_.render_desktop_companion(*cmd, world_, engine_.current_frame(),
                                                   mirror_view);
                engine_.end_frame();
            }

            xr_system_->end_frame(xr_state);
        } else {
            // Desktop-only (existing path, unchanged)
            renderer_.render_frame(world_);
        }
    }
    engine_.wait_idle();
}

} // namespace voxel

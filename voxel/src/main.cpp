#include <voxel/application.hpp>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>

auto main() -> int {
    spdlog::info("Voxel engine starting");
    try {
        voxel::Application app;
        app.run();
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }
    spdlog::info("Voxel engine exited cleanly");
    return EXIT_SUCCESS;
}

add_library(steel)

target_sources(steel PRIVATE
    src/engine.cpp
    src/pipeline.cpp
    src/buffer.cpp
)

target_include_directories(steel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(steel PUBLIC
    Vulkan::Vulkan
    glm::glm
    SDL3::SDL3
    spdlog::spdlog
)

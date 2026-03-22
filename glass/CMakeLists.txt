add_library(glass)

target_sources(glass PRIVATE
    src/camera.cpp
    src/vertex.cpp
    src/shader.cpp
    src/geometry.cpp
    src/material.cpp
    src/renderer.cpp
    src/world.cpp
)

target_include_directories(glass PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(glass PUBLIC
    steel
    glm::glm
)

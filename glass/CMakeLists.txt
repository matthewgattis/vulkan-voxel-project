add_library(glass)

target_sources(glass PRIVATE
    src/vertex.cpp
    src/shader.cpp
    src/geometry.cpp
    src/material.cpp
    src/renderable.cpp
    src/renderer.cpp
)

target_include_directories(glass PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(glass PUBLIC
    steel
    glm::glm
)

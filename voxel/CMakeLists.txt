add_executable(voxel)

target_sources(voxel PRIVATE
    src/main.cpp
    src/application.cpp
    src/camera_controller.cpp
    src/chunk.cpp
    src/chunk_mesh.cpp
    src/chunk_manager.cpp
    src/terrain_generator.cpp
)

target_include_directories(voxel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(voxel PRIVATE
    glass
)

# --- Shader compilation ---

find_program(GLSLC glslc REQUIRED)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SHADERS
    ${SHADER_DIR}/triangle.vert
    ${SHADER_DIR}/triangle.frag
)

foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_NAME}"
    )
    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(voxel_shaders DEPENDS ${SHADER_OUTPUTS})
add_dependencies(voxel voxel_shaders)

target_compile_definitions(voxel PRIVATE
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
)

add_library(steel)

target_sources(steel PRIVATE
    src/engine.cpp
    src/pipeline.cpp
    src/buffer.cpp
    src/vma_impl.cpp
)

target_include_directories(steel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(steel PUBLIC
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
)

target_link_libraries(steel PUBLIC
    Vulkan::Vulkan
    glm::glm
    SDL3::SDL3
    spdlog::spdlog
    imgui::imgui
    GPUOpen::VulkanMemoryAllocator
)

# --- Shader compilation (FXAA post-process) ---

find_program(GLSLC glslc
    HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/shaderc"
    REQUIRED
)

set(STEEL_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(STEEL_SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${STEEL_SHADER_OUTPUT_DIR})

set(STEEL_SHADERS
    ${STEEL_SHADER_DIR}/fullscreen.vert
    ${STEEL_SHADER_DIR}/fxaa.frag
)

foreach(SHADER ${STEEL_SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT ${STEEL_SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling steel shader: ${SHADER_NAME}"
    )
    list(APPEND STEEL_SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

# Generate embedded SPIR-V header
set(STEEL_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/steel)
file(MAKE_DIRECTORY ${STEEL_GENERATED_DIR})

add_custom_command(
    OUTPUT ${STEEL_GENERATED_DIR}/fxaa_shaders.hpp
    COMMAND ${CMAKE_COMMAND}
        "-DINPUT_FILES=${STEEL_SHADER_OUTPUT_DIR}/fullscreen.vert.spv,${STEEL_SHADER_OUTPUT_DIR}/fxaa.frag.spv"
        "-DOUTPUT_FILE=${STEEL_GENERATED_DIR}/fxaa_shaders.hpp"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spirv_to_header.cmake
    DEPENDS ${STEEL_SHADER_OUTPUTS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spirv_to_header.cmake
    COMMENT "Generating embedded SPIR-V header: fxaa_shaders.hpp"
)

add_custom_target(steel_shaders DEPENDS ${STEEL_SHADER_OUTPUTS} ${STEEL_GENERATED_DIR}/fxaa_shaders.hpp)
add_dependencies(steel steel_shaders)

target_include_directories(steel PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

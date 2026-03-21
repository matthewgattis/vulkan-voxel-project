# spirv_to_header.cmake
# Converts SPIR-V binary files to a C++ header with constexpr std::array<uint32_t, N>.
#
# Usage: cmake -P spirv_to_header.cmake -DINPUT_FILES="file1.spv;file2.spv" -DOUTPUT_FILE="path/to/header.hpp"

if(NOT DEFINED INPUT_FILES OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "INPUT_FILES and OUTPUT_FILE must be defined")
endif()

# Support comma-separated file lists (semicolons are hard to pass through CMake custom commands)
string(REPLACE "," ";" INPUT_FILES "${INPUT_FILES}")

set(HEADER_CONTENT "#pragma once\n\n#include <array>\n#include <cstdint>\n\nnamespace steel::shaders {\n\n")

foreach(SPV_FILE ${INPUT_FILES})
    # Derive variable name from filename: fullscreen.vert.spv -> fullscreen_vert
    get_filename_component(BASE_NAME "${SPV_FILE}" NAME)
    # Remove .spv extension
    string(REGEX REPLACE "\\.spv$" "" VAR_NAME "${BASE_NAME}")
    # Replace dots with underscores
    string(REPLACE "." "_" VAR_NAME "${VAR_NAME}")

    # Read binary file
    file(READ "${SPV_FILE}" SPV_HEX HEX)
    string(LENGTH "${SPV_HEX}" HEX_LEN)
    math(EXPR WORD_COUNT "${HEX_LEN} / 8")

    string(APPEND HEADER_CONTENT "constexpr std::array<uint32_t, ${WORD_COUNT}> ${VAR_NAME} = {{\n")

    set(I 0)
    while(I LESS HEX_LEN)
        # Read 4 bytes (8 hex chars) as little-endian uint32_t
        # SPIR-V is little-endian, file(READ ... HEX) gives us bytes in file order
        # We need to swap byte order: bytes AB CD EF GH -> 0xGHEFCDAB
        string(SUBSTRING "${SPV_HEX}" ${I} 2 BYTE0)
        math(EXPR J "${I} + 2")
        string(SUBSTRING "${SPV_HEX}" ${J} 2 BYTE1)
        math(EXPR J "${I} + 4")
        string(SUBSTRING "${SPV_HEX}" ${J} 2 BYTE2)
        math(EXPR J "${I} + 6")
        string(SUBSTRING "${SPV_HEX}" ${J} 2 BYTE3)

        set(WORD "0x${BYTE3}${BYTE2}${BYTE1}${BYTE0}")

        math(EXPR NEXT_I "${I} + 8")
        if(NEXT_I LESS HEX_LEN)
            string(APPEND HEADER_CONTENT "    ${WORD},\n")
        else()
            string(APPEND HEADER_CONTENT "    ${WORD},\n")
        endif()

        set(I ${NEXT_I})
    endwhile()

    string(APPEND HEADER_CONTENT "}};\n\n")
endforeach()

string(APPEND HEADER_CONTENT "} // namespace steel::shaders\n")

file(WRITE "${OUTPUT_FILE}" "${HEADER_CONTENT}")

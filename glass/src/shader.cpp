#include <glass/shader.hpp>

#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>

namespace glass {

Shader::Shader(vk::ShaderStageFlagBits stage, std::vector<uint32_t> spirv)
    : stage_(stage)
    , spirv_(std::move(spirv)) {}

Shader Shader::load(vk::ShaderStageFlagBits stage,
                    const std::filesystem::path& spirv_path) {
    std::ifstream file(spirv_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open SPIR-V file: " + spirv_path.string());
    }

    auto file_size = file.tellg();
    if (file_size % sizeof(uint32_t) != 0) {
        throw std::runtime_error("SPIR-V file size is not a multiple of 4: " + spirv_path.string());
    }

    std::vector<uint32_t> spirv(static_cast<size_t>(file_size) / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirv.data()), file_size);

    auto stage_name = (stage == vk::ShaderStageFlagBits::eVertex) ? "vertex"
                    : (stage == vk::ShaderStageFlagBits::eFragment) ? "fragment"
                    : "unknown";
    spdlog::info("Shader loaded: {} ({}, {} bytes)", spirv_path.filename().string(), stage_name, static_cast<size_t>(file_size));

    return Shader(stage, std::move(spirv));
}

} // namespace glass

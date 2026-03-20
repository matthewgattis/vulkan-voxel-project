#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <filesystem>
#include <vector>
#include <span>

namespace glass {

class Shader {
public:
    static Shader load(vk::ShaderStageFlagBits stage,
                       const std::filesystem::path& spirv_path);

    vk::ShaderStageFlagBits stage() const { return stage_; }
    std::span<const uint32_t> spirv() const { return spirv_; }

    Shader(Shader&&) = default;
    Shader& operator=(Shader&&) = default;

private:
    Shader(vk::ShaderStageFlagBits stage, std::vector<uint32_t> spirv);

    vk::ShaderStageFlagBits stage_;
    std::vector<uint32_t> spirv_;
};

} // namespace glass

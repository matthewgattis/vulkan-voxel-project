#pragma once

#include <glass/shader.hpp>
#include <steel/engine.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace glass {

class Material {
public:
    static Material create(steel::Engine& engine,
                           const Shader& vertex_shader,
                           const Shader& fragment_shader);

    void bind(const vk::raii::CommandBuffer& cmd) const;

    Material(Material&&) = default;
    Material& operator=(Material&&) = default;

private:
    Material(vk::raii::PipelineLayout layout, vk::raii::Pipeline pipeline);

    vk::raii::PipelineLayout layout_;
    vk::raii::Pipeline pipeline_;
};

} // namespace glass

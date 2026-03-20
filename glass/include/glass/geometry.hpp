#pragma once

#include <glass/mesh.hpp>
#include <steel/buffer.hpp>
#include <steel/engine.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <optional>

namespace glass {

class Geometry {
public:
    static Geometry create(steel::Engine& engine, const Mesh& mesh);

    void bind(const vk::raii::CommandBuffer& cmd) const;
    void draw(const vk::raii::CommandBuffer& cmd) const;

    Geometry(Geometry&&) = default;
    Geometry& operator=(Geometry&&) = default;

private:
    Geometry(steel::Buffer vertex_buffer, std::optional<steel::Buffer> index_buffer,
             uint32_t vertex_count, uint32_t index_count);

    steel::Buffer vertex_buffer_;
    std::optional<steel::Buffer> index_buffer_;
    uint32_t vertex_count_;
    uint32_t index_count_;
};

} // namespace glass

#include <glass/geometry.hpp>

#include <spdlog/spdlog.h>

namespace glass {

Geometry::Geometry(steel::Buffer vertex_buffer, std::optional<steel::Buffer> index_buffer,
                   uint32_t vertex_count, uint32_t index_count)
    : vertex_buffer_(std::move(vertex_buffer))
    , index_buffer_(std::move(index_buffer))
    , vertex_count_(vertex_count)
    , index_count_(index_count) {}

Geometry Geometry::create(steel::Engine& engine, const Mesh& mesh) {
    auto vertices = mesh.vertices();
    auto indices = mesh.indices();

    auto vertex_buffer = steel::Buffer::create_vertex_buffer(
        engine.device(),
        engine.physical_device(),
        engine.command_pool(),
        engine.graphics_queue(),
        std::as_bytes(vertices));

    std::optional<steel::Buffer> index_buffer;
    if (!indices.empty()) {
        index_buffer = steel::Buffer::create_index_buffer(
            engine.device(),
            engine.physical_device(),
            engine.command_pool(),
            engine.graphics_queue(),
            std::as_bytes(indices));
    }

    auto vtx_count = static_cast<uint32_t>(vertices.size());
    auto idx_count = static_cast<uint32_t>(indices.size());
    spdlog::info("Geometry created: {} vertices, {} indices", vtx_count, idx_count);

    return Geometry(std::move(vertex_buffer), std::move(index_buffer), vtx_count, idx_count);
}

void Geometry::bind(const vk::raii::CommandBuffer& cmd) const {
    std::array<vk::Buffer, 1> buffers{vertex_buffer_.handle()};
    std::array<vk::DeviceSize, 1> offsets{0};
    cmd.bindVertexBuffers(0, buffers, offsets);

    if (index_buffer_) {
        cmd.bindIndexBuffer(index_buffer_->handle(), 0, vk::IndexType::eUint32);
    }
}

void Geometry::draw(const vk::raii::CommandBuffer& cmd) const {
    if (index_buffer_) {
        cmd.drawIndexed(index_count_, 1, 0, 0, 0);
    } else {
        cmd.draw(vertex_count_, 1, 0, 0);
    }
}

} // namespace glass

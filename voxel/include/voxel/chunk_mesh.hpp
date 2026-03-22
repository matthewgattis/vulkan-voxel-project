#pragma once

#include <voxel/chunk.hpp>

#include <glass/mesh.hpp>

#include <functional>
#include <span>
#include <vector>

namespace voxel {

using SolidQuery = std::function<bool(int wx, int wy, int wz)>;

class ChunkMesh : public glass::Mesh {
public:
    ChunkMesh(const Chunk& chunk, const SolidQuery& is_solid_at);

    std::span<const glass::Vertex> vertices() const override { return vertices_; }
    std::span<const uint32_t> indices() const override { return indices_; }

    bool empty() const { return vertices_.empty(); }

private:
    std::vector<glass::Vertex> vertices_;
    std::vector<uint32_t> indices_;
};

} // namespace voxel

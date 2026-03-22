#include <voxel/chunk.hpp>

namespace voxel {

Chunk::Chunk(int cx, int cy, int cz)
    : cx_{cx}, cy_{cy}, cz_{cz}
{}

int Chunk::index(int x, int y, int z) {
    return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
}

bool Chunk::in_bounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE &&
           y >= 0 && y < CHUNK_SIZE &&
           z >= 0 && z < CHUNK_SIZE;
}

VoxelType Chunk::get(int x, int y, int z) const {
    if (!in_bounds(x, y, z)) return VoxelType::Air;
    return voxels_[static_cast<size_t>(index(x, y, z))];
}

void Chunk::set(int x, int y, int z, VoxelType v) {
    if (!in_bounds(x, y, z)) return;
    voxels_[static_cast<size_t>(index(x, y, z))] = v;
}

} // namespace voxel

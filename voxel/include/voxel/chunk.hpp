#pragma once

#include <voxel/voxel.hpp>

#include <array>
#include <cstdint>

namespace voxel {

inline constexpr int CHUNK_SIZE = 16;
inline constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

class Chunk {
public:
    Chunk(int cx, int cy, int cz);

    VoxelType get(int x, int y, int z) const;
    void set(int x, int y, int z, VoxelType v);
    bool in_bounds(int x, int y, int z) const;

    int cx() const { return cx_; }
    int cy() const { return cy_; }
    int cz() const { return cz_; }

private:
    static int index(int x, int y, int z);

    int cx_, cy_, cz_;
    std::array<VoxelType, CHUNK_VOLUME> voxels_{};
};

} // namespace voxel

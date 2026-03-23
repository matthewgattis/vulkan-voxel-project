#pragma once

#include <voxel/chunk.hpp>

namespace voxel {

class TerrainGenerator {
public:
    void fill_chunk(Chunk& chunk) const;
    bool is_solid_at(int wx, int wy, int wz) const;
    int column_height() const;

private:
    static constexpr float NOISE_SCALE = 0.05f;
    static constexpr float HEIGHT_BASE = 16.0f;
    static constexpr float HEIGHT_AMP = 12.0f;

    float terrain_height(float wx, float wy) const;
};

} // namespace voxel

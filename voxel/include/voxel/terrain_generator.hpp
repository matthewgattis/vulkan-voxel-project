#pragma once

#include <voxel/chunk.hpp>

#include <array>

namespace voxel {

class TerrainGenerator {
public:
    static constexpr float SEA_LEVEL = 85.0f;

    float height_at(float wx, float wy) const;
    bool is_solid_at(int wx, int wy, int wz) const;
    bool is_opaque_at(int wx, int wy, int wz) const;

private:
    static constexpr int OCTAVES = 6;
    static constexpr float BASE_FREQUENCY = 0.005f;
    static constexpr float LACUNARITY = 2.0f;
    static constexpr float PERSISTENCE = 0.5f;
    static constexpr float HEIGHT_BASE = 48.0f;
    static constexpr float HEIGHT_AMP = 80.0f;
};

class TerrainColumn {
public:
    TerrainColumn(int cx, int cy, const TerrainGenerator& generator);

    int min_slice() const { return min_slice_; }
    int max_slice() const { return max_slice_; }

    void fill_chunk(Chunk& chunk) const;
    bool is_solid_at(int lx, int ly, int wz) const;
    bool is_opaque_at(int lx, int ly, int wz) const;

private:
    int cx_, cy_;
    int min_slice_, max_slice_;
    std::array<float, CHUNK_SIZE * CHUNK_SIZE> heights_;
    std::array<float, CHUNK_SIZE * CHUNK_SIZE> slopes_;
};

} // namespace voxel

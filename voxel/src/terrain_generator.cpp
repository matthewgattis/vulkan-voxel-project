#include <voxel/terrain_generator.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/noise.hpp>

#include <cmath>

namespace voxel {

float TerrainGenerator::terrain_height(float wx, float wy) const {
    float n = glm::simplex(glm::vec2(wx * NOISE_SCALE, wy * NOISE_SCALE));
    n = (n + 1.0f) * 0.5f;
    return HEIGHT_BASE + n * HEIGHT_AMP;
}

bool TerrainGenerator::is_solid_at(int wx, int wy, int wz) const {
    if (wz < 0) return true;
    float height = terrain_height(static_cast<float>(wx), static_cast<float>(wy));
    return static_cast<float>(wz) < height;
}

void TerrainGenerator::fill_chunk(Chunk& chunk) const {
    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            float wx = static_cast<float>(chunk.cx() * CHUNK_SIZE + lx);
            float wy = static_cast<float>(chunk.cy() * CHUNK_SIZE + ly);
            float height = terrain_height(wx, wy);

            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                float wz = static_cast<float>(chunk.cz() * CHUNK_SIZE + lz);
                if (wz >= height) continue;

                float depth = height - wz;
                VoxelType type;
                if (depth <= 1.0f) {
                    type = VoxelType::Grass;
                } else if (depth <= 4.0f) {
                    type = VoxelType::Dirt;
                } else {
                    type = VoxelType::Stone;
                }
                chunk.set(lx, ly, lz, type);
            }
        }
    }
}

int TerrainGenerator::column_height() const {
    return static_cast<int>(
        std::ceil((HEIGHT_BASE + HEIGHT_AMP) / static_cast<float>(CHUNK_SIZE)));
}

} // namespace voxel

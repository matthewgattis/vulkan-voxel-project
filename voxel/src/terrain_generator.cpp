#include <voxel/terrain_generator.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/noise.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace voxel {

// ---------------------------------------------------------------------------
// TerrainGenerator
// ---------------------------------------------------------------------------

float TerrainGenerator::height_at(float wx, float wy) const {
    float amplitude = 1.0f;
    float frequency = BASE_FREQUENCY;
    float total = 0.0f;
    float max_amplitude = 0.0f;

    for (int i = 0; i < OCTAVES; ++i) {
        total += amplitude * glm::simplex(glm::vec2(wx * frequency, wy * frequency));
        max_amplitude += amplitude;
        amplitude *= PERSISTENCE;
        frequency *= LACUNARITY;
    }

    float n = total / max_amplitude;

    if (n > 0.0f) {
        n = glm::pow(n, 2.0f);
    }

    return n * HEIGHT_AMP;
}

float TerrainGenerator::snow_line_at(float wx, float wy) const {
    float n = glm::simplex(glm::vec2(wx * 0.003f, wy * 0.003f));
    return SNOW_LINE + n * 15.0f;
}

bool TerrainGenerator::is_solid_at(int wx, int wy, int wz) const {
    float fwz = static_cast<float>(wz);
    return fwz < height_at(static_cast<float>(wx), static_cast<float>(wy));
}

bool TerrainGenerator::is_opaque_at(int wx, int wy, int wz) const {
    float fwz = static_cast<float>(wz);
    float h = height_at(static_cast<float>(wx), static_cast<float>(wy));
    return fwz < h || fwz < SEA_LEVEL;
}

// ---------------------------------------------------------------------------
// TerrainColumn
// ---------------------------------------------------------------------------

TerrainColumn::TerrainColumn(int cx, int cy, const TerrainGenerator& generator)
    : cx_{cx}, cy_{cy}
{
    float min_h = std::numeric_limits<float>::max();
    float max_h = std::numeric_limits<float>::lowest();

    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            float wx = static_cast<float>(cx * CHUNK_SIZE + lx);
            float wy = static_cast<float>(cy * CHUNK_SIZE + ly);
            float h = generator.height_at(wx, wy);
            heights_[lx + ly * CHUNK_SIZE] = h;
            min_h = std::min(min_h, h);
            max_h = std::max(max_h, h);

            // Gradient via central differences
            float hx = generator.height_at(wx + 1.0f, wy) - generator.height_at(wx - 1.0f, wy);
            float hy = generator.height_at(wx, wy + 1.0f) - generator.height_at(wx, wy - 1.0f);
            slopes_[lx + ly * CHUNK_SIZE] = std::sqrt(hx * hx + hy * hy) * 0.5f;

            snow_lines_[lx + ly * CHUNK_SIZE] = generator.snow_line_at(wx, wy);
        }
    }

    // Account for sea level — water extends the surface range
    float effective_max = std::max(max_h, TerrainGenerator::SEA_LEVEL);

    // Slices where the surface passes through: need one below min for AO/culling
    int floor_z = static_cast<int>(std::floor(min_h));
    min_slice_ = (floor_z < 0 ? (floor_z - CHUNK_SIZE + 1) / CHUNK_SIZE : floor_z / CHUNK_SIZE) - 1;
    max_slice_ = static_cast<int>(std::ceil(effective_max)) / CHUNK_SIZE;
}

void TerrainColumn::fill_chunk(Chunk& chunk) const {
    int oz = chunk.cz() * CHUNK_SIZE;

    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            int idx = lx + ly * CHUNK_SIZE;
            float height = heights_[idx];
            float slope = slopes_[idx];
            bool underwater = height < TerrainGenerator::SEA_LEVEL;

            // Sand band: extends further on gentle slopes
            // Below sea level: sand in shallows
            float above_band = std::max(0.0f, 2.0f - slope * 1.0f);
            float below_band = 2.0f;

            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                float wz = static_cast<float>(oz + lz);

                if (wz < height) {
                    float depth = height - wz;
                    float dist_to_sea = height - TerrainGenerator::SEA_LEVEL;
                    bool in_sand_zone = (dist_to_sea >= 0.0f && dist_to_sea < above_band) ||
                                        (dist_to_sea < 0.0f && dist_to_sea > -below_band);

                    float snow_line = snow_lines_[idx];
                    bool above_snow = height > snow_line;
                    // Snow covers gentle slopes; steeper slopes get less snow
                    float snow_depth = std::max(0.0f, 3.0f - slope * 1.5f);

                    // Grass thins out approaching the snow line — exposed stone above tree line
                    // High-frequency noise breaks up the transition
                    float wx = static_cast<float>(cx_ * CHUNK_SIZE + lx);
                    float wy = static_cast<float>(cy_ * CHUNK_SIZE + ly);
                    float tree_noise = glm::simplex(glm::vec2(wx * 0.1f, wy * 0.1f)) * 5.0f;
                    float tree_line = snow_line - 10.0f + tree_noise;
                    bool has_grass = !underwater && height <= tree_line;

                    VoxelType type;
                    if (in_sand_zone && depth <= 4.0f) {
                        type = VoxelType::Sand;
                    } else if (above_snow && depth <= snow_depth) {
                        type = VoxelType::Snow;
                    } else if (depth <= 1.0f && has_grass) {
                        type = VoxelType::Grass;
                    } else if (depth <= 4.0f && has_grass) {
                        type = VoxelType::Dirt;
                    } else {
                        type = VoxelType::Stone;
                    }
                    chunk.set(lx, ly, lz, type);
                } else if (wz < TerrainGenerator::SEA_LEVEL) {
                    // Water above terrain
                    chunk.set(lx, ly, lz, VoxelType::Water);
                }
            }
        }
    }
}

bool TerrainColumn::is_solid_at(int lx, int ly, int wz) const {
    float fwz = static_cast<float>(wz);
    return fwz < heights_[lx + ly * CHUNK_SIZE];
}

bool TerrainColumn::is_opaque_at(int lx, int ly, int wz) const {
    float fwz = static_cast<float>(wz);
    return fwz < heights_[lx + ly * CHUNK_SIZE] || fwz < TerrainGenerator::SEA_LEVEL;
}

} // namespace voxel

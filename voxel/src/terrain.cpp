#include <voxel/terrain.hpp>
#include <voxel/chunk_mesh.hpp>

#include <glass/components.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/noise.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace voxel {

float Terrain::terrain_height(float wx, float wy) {
    float n = glm::simplex(glm::vec2(wx * NOISE_SCALE, wy * NOISE_SCALE));
    // Remap from [-1,1] to [0,1]
    n = (n + 1.0f) * 0.5f;
    return HEIGHT_BASE + n * HEIGHT_AMP;
}

bool Terrain::is_solid_at(int wx, int wy, int wz) {
    if (wz < 0) return true;  // below world is solid
    float height = terrain_height(static_cast<float>(wx), static_cast<float>(wy));
    return static_cast<float>(wz) < height;
}

void Terrain::fill_chunk(Chunk& chunk) {
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

Terrain::Terrain(steel::Engine& engine, glass::World& world, const glass::Material& material) {
    int max_z_chunks = static_cast<int>(
        std::ceil((HEIGHT_BASE + HEIGHT_AMP) / static_cast<float>(CHUNK_SIZE)));

    for (int cx = 0; cx < GRID_X; ++cx) {
        for (int cy = 0; cy < GRID_Y; ++cy) {
            for (int cz = 0; cz < max_z_chunks; ++cz) {
                Chunk chunk{cx, cy, cz};
                fill_chunk(chunk);

                ChunkMesh chunk_mesh{chunk, is_solid_at};

                if (chunk_mesh.empty()) continue;
                auto geom = std::make_unique<glass::Geometry>(
                    glass::Geometry::create(engine, chunk_mesh));
                geometries_.push_back(std::move(geom));

                auto entity = world.create();
                glm::mat4 chunk_transform = glm::translate(
                    glm::mat4{1.0f},
                    glm::vec3{
                        static_cast<float>(cx * CHUNK_SIZE),
                        static_cast<float>(cy * CHUNK_SIZE),
                        static_cast<float>(cz * CHUNK_SIZE)
                    });
                world.add<glass::Transform>(entity, glass::Transform{chunk_transform});
                world.add<glass::MeshComponent>(entity,
                    glass::MeshComponent{geometries_.back().get()});
                world.add<glass::MaterialComponent>(entity,
                    glass::MaterialComponent{&material});
            }
        }
    }
}

} // namespace voxel

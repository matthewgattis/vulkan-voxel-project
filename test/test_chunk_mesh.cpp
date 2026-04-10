#include <voxel/chunk_mesh.hpp>

#include <gtest/gtest.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

using namespace voxel;

// Helper queries
static VoxelQuery always_false = [](int, int, int) { return false; };

// Build an opaque query that reflects a chunk's contents
static VoxelQuery opaque_from_chunk(const Chunk& chunk) {
    int ox = chunk.cx() * CHUNK_SIZE;
    int oy = chunk.cy() * CHUNK_SIZE;
    int oz = chunk.cz() * CHUNK_SIZE;
    return [&chunk, ox, oy, oz](int wx, int wy, int wz) {
        return is_opaque(chunk.get(wx - ox, wy - oy, wz - oz));
    };
}

TEST(ChunkMesh, EmptyChunkProducesNoMesh) {
    Chunk chunk(0, 0, 0);
    ChunkMesh mesh(chunk, always_false, always_false);
    EXPECT_TRUE(mesh.empty());
    EXPECT_TRUE(mesh.vertices().empty());
    EXPECT_TRUE(mesh.indices().empty());
}

TEST(ChunkMesh, SingleVoxelProducesFaces) {
    Chunk chunk(0, 0, 0);
    chunk.set(8, 8, 8, VoxelType::Stone);

    ChunkMesh mesh(chunk, always_false, always_false);
    EXPECT_FALSE(mesh.empty());

    // A single voxel with no opaque neighbors should have 6 faces
    // Each face = 2 triangles = 6 indices
    EXPECT_EQ(mesh.indices().size(), 36u);
}

TEST(ChunkMesh, FullChunkOnlyHasSurfaceFaces) {
    Chunk chunk(0, 0, 0);
    for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int y = 0; y < CHUNK_SIZE; ++y)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                chunk.set(x, y, z, VoxelType::Stone);

    // Use chunk-aware opaque query so internal faces are culled
    auto opaque = opaque_from_chunk(chunk);
    ChunkMesh mesh(chunk, opaque, always_false);
    EXPECT_FALSE(mesh.empty());

    // Interior faces are culled — only surface faces remain.
    // Each face of the cube has CHUNK_SIZE * CHUNK_SIZE quads = 6 indices each.
    size_t expected_indices = 6 * CHUNK_SIZE * CHUNK_SIZE * 6;
    EXPECT_EQ(mesh.indices().size(), expected_indices);
}

TEST(ChunkMesh, AdjacentOpaqueNeighborCullsFace) {
    Chunk chunk(0, 0, 0);
    chunk.set(0, 0, 0, VoxelType::Stone);

    // Simulate an opaque neighbor in the +X direction
    auto opaque_plus_x = [](int wx, int wy, int wz) {
        return wx == 1 && wy == 0 && wz == 0;
    };

    ChunkMesh mesh(chunk, opaque_plus_x, always_false);
    // Should have 5 faces instead of 6
    EXPECT_EQ(mesh.indices().size(), 30u);
}

TEST(ChunkMesh, TwoAdjacentVoxelsShareCulledFace) {
    Chunk chunk(0, 0, 0);
    chunk.set(5, 5, 5, VoxelType::Stone);
    chunk.set(6, 5, 5, VoxelType::Stone);

    auto opaque = opaque_from_chunk(chunk);
    ChunkMesh mesh(chunk, opaque, always_false);
    // Two voxels: 12 total faces, minus 2 shared faces = 10 visible faces
    EXPECT_EQ(mesh.indices().size(), 60u);
}

TEST(ChunkMesh, WaterVoxelProducesFaces) {
    Chunk chunk(0, 0, 0);
    chunk.set(0, 0, 0, VoxelType::Water);

    ChunkMesh mesh(chunk, always_false, always_false);
    EXPECT_FALSE(mesh.empty());
    EXPECT_EQ(mesh.indices().size(), 36u);
}

TEST(ChunkMesh, IndicesReferenceValidVertices) {
    Chunk chunk(0, 0, 0);
    chunk.set(4, 4, 4, VoxelType::Grass);
    chunk.set(5, 4, 4, VoxelType::Dirt);

    ChunkMesh mesh(chunk, always_false, always_false);
    auto verts = mesh.vertices();
    for (auto idx : mesh.indices()) {
        EXPECT_LT(idx, verts.size());
    }
}

TEST(ChunkMesh, IndexCountIsMultipleOfThree) {
    Chunk chunk(0, 0, 0);
    chunk.set(0, 0, 0, VoxelType::Stone);
    chunk.set(1, 0, 0, VoxelType::Dirt);
    chunk.set(0, 1, 0, VoxelType::Grass);

    ChunkMesh mesh(chunk, always_false, always_false);
    EXPECT_EQ(mesh.indices().size() % 3, 0u);
}

TEST(ChunkMesh, VerticesHaveNonZeroNormals) {
    Chunk chunk(0, 0, 0);
    chunk.set(8, 8, 8, VoxelType::Stone);

    ChunkMesh mesh(chunk, always_false, always_false);
    for (const auto& v : mesh.vertices()) {
        float len = glm::length(v.normal);
        EXPECT_NEAR(len, 1.0f, 0.001f);
    }
}

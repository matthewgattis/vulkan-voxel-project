#include <voxel/terrain_generator.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace voxel;

// --- TerrainGenerator ---

TEST(TerrainGenerator, HeightAtOriginIsFinite) {
    TerrainGenerator gen;
    float h = gen.height_at(0.0f, 0.0f);
    EXPECT_TRUE(std::isfinite(h));
}

TEST(TerrainGenerator, HeightBounded) {
    TerrainGenerator gen;
    // Sample many points — height should stay within [-HEIGHT_AMP, HEIGHT_AMP]
    for (int i = -50; i < 50; ++i) {
        for (int j = -50; j < 50; ++j) {
            float h = gen.height_at(static_cast<float>(i * 10), static_cast<float>(j * 10));
            EXPECT_GE(h, -TerrainGenerator::HEIGHT_AMP);
            EXPECT_LE(h, TerrainGenerator::HEIGHT_AMP);
        }
    }
}

TEST(TerrainGenerator, HeightIsDeterministic) {
    TerrainGenerator gen;
    float h1 = gen.height_at(100.0f, 200.0f);
    float h2 = gen.height_at(100.0f, 200.0f);
    EXPECT_FLOAT_EQ(h1, h2);
}

TEST(TerrainGenerator, HeightVariesSpatially) {
    TerrainGenerator gen;
    // At sufficiently different positions, heights should differ
    float h1 = gen.height_at(0.0f, 0.0f);
    float h2 = gen.height_at(1000.0f, 1000.0f);
    EXPECT_NE(h1, h2);
}

TEST(TerrainGenerator, SnowLineIsFinite) {
    TerrainGenerator gen;
    float s = gen.snow_line_at(0.0f, 0.0f);
    EXPECT_TRUE(std::isfinite(s));
}

TEST(TerrainGenerator, SnowLineNearBaseValue) {
    TerrainGenerator gen;
    // Snow line should be within ±15 of SNOW_LINE
    for (int i = -10; i < 10; ++i) {
        float s = gen.snow_line_at(static_cast<float>(i * 100), 0.0f);
        EXPECT_GE(s, TerrainGenerator::SNOW_LINE - 15.0f);
        EXPECT_LE(s, TerrainGenerator::SNOW_LINE + 15.0f);
    }
}

TEST(TerrainGenerator, IsSolidBelowSurface) {
    TerrainGenerator gen;
    float h = gen.height_at(0.0f, 0.0f);
    int well_below = static_cast<int>(std::floor(h)) - 10;
    EXPECT_TRUE(gen.is_solid_at(0, 0, well_below));
}

TEST(TerrainGenerator, IsNotSolidAboveSurface) {
    TerrainGenerator gen;
    float h = gen.height_at(0.0f, 0.0f);
    int well_above = static_cast<int>(std::ceil(h)) + 10;
    EXPECT_FALSE(gen.is_solid_at(0, 0, well_above));
}

TEST(TerrainGenerator, IsOpaqueIncludesWater) {
    TerrainGenerator gen;
    // Find a point where height is below sea level
    // At sea level z=0, is_opaque should be true (water fills below SEA_LEVEL)
    float h = gen.height_at(0.0f, 0.0f);
    if (h < TerrainGenerator::SEA_LEVEL) {
        int z = static_cast<int>(std::ceil(h));
        if (z < static_cast<int>(TerrainGenerator::SEA_LEVEL)) {
            EXPECT_TRUE(gen.is_opaque_at(0, 0, z));
        }
    }
}

TEST(TerrainGenerator, IsNotOpaqueHighAboveSurface) {
    TerrainGenerator gen;
    // Well above both terrain and sea level
    EXPECT_FALSE(gen.is_opaque_at(0, 0, 200));
}

// --- TerrainColumn ---

TEST(TerrainColumn, MinSliceLessThanMaxSlice) {
    TerrainGenerator gen;
    TerrainColumn col(0, 0, gen);
    EXPECT_LE(col.min_slice(), col.max_slice());
}

TEST(TerrainColumn, FillChunkProducesSolidVoxels) {
    TerrainGenerator gen;
    TerrainColumn col(0, 0, gen);

    // Fill a chunk at the min slice (should contain some solid voxels)
    Chunk chunk(0, 0, col.min_slice() + 1);
    col.fill_chunk(chunk);

    bool has_solid = false;
    for (int z = 0; z < CHUNK_SIZE && !has_solid; ++z)
        for (int y = 0; y < CHUNK_SIZE && !has_solid; ++y)
            for (int x = 0; x < CHUNK_SIZE && !has_solid; ++x)
                if (chunk.get(x, y, z) != VoxelType::Air)
                    has_solid = true;
    EXPECT_TRUE(has_solid);
}

TEST(TerrainColumn, ChunkAboveMaxSliceIsEmpty) {
    TerrainGenerator gen;
    TerrainColumn col(0, 0, gen);

    Chunk chunk(0, 0, col.max_slice() + 5);
    col.fill_chunk(chunk);

    for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int y = 0; y < CHUNK_SIZE; ++y)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                EXPECT_EQ(chunk.get(x, y, z), VoxelType::Air);
}

TEST(TerrainColumn, IsSolidConsistentWithGenerator) {
    TerrainGenerator gen;
    TerrainColumn col(0, 0, gen);

    // The column's is_solid_at should agree with the generator for the same position
    int wx = 5;
    int wy = 5;
    for (int wz = -20; wz < 100; ++wz) {
        EXPECT_EQ(col.is_solid_at(wx, wy, wz), gen.is_solid_at(wx, wy, wz));
    }
}

TEST(TerrainColumn, FillChunkContainsExpectedTypes) {
    TerrainGenerator gen;
    TerrainColumn col(0, 0, gen);

    // Fill chunks across the surface range and verify we get real voxel types
    bool found_stone = false;
    for (int cz = col.min_slice(); cz <= col.max_slice(); ++cz) {
        Chunk chunk(0, 0, cz);
        col.fill_chunk(chunk);
        for (int z = 0; z < CHUNK_SIZE && !found_stone; ++z)
            for (int y = 0; y < CHUNK_SIZE && !found_stone; ++y)
                for (int x = 0; x < CHUNK_SIZE && !found_stone; ++x)
                    if (chunk.get(x, y, z) == VoxelType::Stone)
                        found_stone = true;
    }
    // Deep underground should always have stone
    EXPECT_TRUE(found_stone);
}

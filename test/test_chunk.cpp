#include <voxel/chunk.hpp>

#include <gtest/gtest.h>

using namespace voxel;

TEST(Chunk, DefaultVoxelsAreAir) {
    Chunk chunk(0, 0, 0);
    for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int y = 0; y < CHUNK_SIZE; ++y)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                EXPECT_EQ(chunk.get(x, y, z), VoxelType::Air);
}

TEST(Chunk, SetAndGet) {
    Chunk chunk(0, 0, 0);
    chunk.set(3, 5, 7, VoxelType::Stone);
    EXPECT_EQ(chunk.get(3, 5, 7), VoxelType::Stone);
}

TEST(Chunk, SetDoesNotAffectOtherVoxels) {
    Chunk chunk(0, 0, 0);
    chunk.set(0, 0, 0, VoxelType::Grass);
    EXPECT_EQ(chunk.get(1, 0, 0), VoxelType::Air);
    EXPECT_EQ(chunk.get(0, 1, 0), VoxelType::Air);
    EXPECT_EQ(chunk.get(0, 0, 1), VoxelType::Air);
}

TEST(Chunk, OverwriteVoxel) {
    Chunk chunk(0, 0, 0);
    chunk.set(5, 5, 5, VoxelType::Dirt);
    chunk.set(5, 5, 5, VoxelType::Sand);
    EXPECT_EQ(chunk.get(5, 5, 5), VoxelType::Sand);
}

TEST(Chunk, InBoundsCorners) {
    Chunk chunk(0, 0, 0);
    EXPECT_TRUE(chunk.in_bounds(0, 0, 0));
    EXPECT_TRUE(chunk.in_bounds(CHUNK_SIZE - 1, CHUNK_SIZE - 1, CHUNK_SIZE - 1));
}

TEST(Chunk, OutOfBoundsNegative) {
    Chunk chunk(0, 0, 0);
    EXPECT_FALSE(chunk.in_bounds(-1, 0, 0));
    EXPECT_FALSE(chunk.in_bounds(0, -1, 0));
    EXPECT_FALSE(chunk.in_bounds(0, 0, -1));
}

TEST(Chunk, OutOfBoundsOverflow) {
    Chunk chunk(0, 0, 0);
    EXPECT_FALSE(chunk.in_bounds(CHUNK_SIZE, 0, 0));
    EXPECT_FALSE(chunk.in_bounds(0, CHUNK_SIZE, 0));
    EXPECT_FALSE(chunk.in_bounds(0, 0, CHUNK_SIZE));
}

TEST(Chunk, GetOutOfBoundsReturnsAir) {
    Chunk chunk(0, 0, 0);
    chunk.set(0, 0, 0, VoxelType::Stone);
    EXPECT_EQ(chunk.get(-1, 0, 0), VoxelType::Air);
    EXPECT_EQ(chunk.get(CHUNK_SIZE, 0, 0), VoxelType::Air);
}

TEST(Chunk, SetOutOfBoundsIsIgnored) {
    Chunk chunk(0, 0, 0);
    chunk.set(-1, 0, 0, VoxelType::Stone);  // should not crash
    chunk.set(CHUNK_SIZE, 0, 0, VoxelType::Stone);
    EXPECT_EQ(chunk.get(0, 0, 0), VoxelType::Air);  // nothing was corrupted
}

TEST(Chunk, ChunkCoordinatesPreserved) {
    Chunk chunk(3, -2, 7);
    EXPECT_EQ(chunk.cx(), 3);
    EXPECT_EQ(chunk.cy(), -2);
    EXPECT_EQ(chunk.cz(), 7);
}

TEST(Chunk, AllVoxelPositionsAddressable) {
    Chunk chunk(0, 0, 0);
    int count = 0;
    for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int y = 0; y < CHUNK_SIZE; ++y)
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                chunk.set(x, y, z, VoxelType::Stone);
                ++count;
            }
    EXPECT_EQ(count, CHUNK_VOLUME);

    for (int z = 0; z < CHUNK_SIZE; ++z)
        for (int y = 0; y < CHUNK_SIZE; ++y)
            for (int x = 0; x < CHUNK_SIZE; ++x)
                EXPECT_EQ(chunk.get(x, y, z), VoxelType::Stone);
}

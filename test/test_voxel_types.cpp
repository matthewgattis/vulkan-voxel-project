#include <voxel/voxel.hpp>

#include <gtest/gtest.h>

using namespace voxel;

// --- is_solid ---

TEST(VoxelType, AirIsNotSolid) {
    EXPECT_FALSE(is_solid(VoxelType::Air));
}

TEST(VoxelType, WaterIsNotSolid) {
    EXPECT_FALSE(is_solid(VoxelType::Water));
}

TEST(VoxelType, GrassIsSolid) {
    EXPECT_TRUE(is_solid(VoxelType::Grass));
}

TEST(VoxelType, DirtIsSolid) {
    EXPECT_TRUE(is_solid(VoxelType::Dirt));
}

TEST(VoxelType, StoneIsSolid) {
    EXPECT_TRUE(is_solid(VoxelType::Stone));
}

TEST(VoxelType, SandIsSolid) {
    EXPECT_TRUE(is_solid(VoxelType::Sand));
}

TEST(VoxelType, SnowIsSolid) {
    EXPECT_TRUE(is_solid(VoxelType::Snow));
}

// --- is_opaque ---

TEST(VoxelType, AirIsNotOpaque) {
    EXPECT_FALSE(is_opaque(VoxelType::Air));
}

TEST(VoxelType, WaterIsOpaque) {
    EXPECT_TRUE(is_opaque(VoxelType::Water));
}

TEST(VoxelType, SolidTypesAreOpaque) {
    EXPECT_TRUE(is_opaque(VoxelType::Grass));
    EXPECT_TRUE(is_opaque(VoxelType::Dirt));
    EXPECT_TRUE(is_opaque(VoxelType::Stone));
    EXPECT_TRUE(is_opaque(VoxelType::Sand));
    EXPECT_TRUE(is_opaque(VoxelType::Snow));
}

// --- voxel_color ---

TEST(VoxelType, AirColorIsBlack) {
    auto c = voxel_color(VoxelType::Air);
    EXPECT_FLOAT_EQ(c.x, 0.0f);
    EXPECT_FLOAT_EQ(c.y, 0.0f);
    EXPECT_FLOAT_EQ(c.z, 0.0f);
}

TEST(VoxelType, EachTypeHasDistinctColor) {
    auto grass = voxel_color(VoxelType::Grass);
    auto dirt  = voxel_color(VoxelType::Dirt);
    auto stone = voxel_color(VoxelType::Stone);
    auto sand  = voxel_color(VoxelType::Sand);
    auto snow  = voxel_color(VoxelType::Snow);
    auto water = voxel_color(VoxelType::Water);

    EXPECT_NE(grass, dirt);
    EXPECT_NE(grass, stone);
    EXPECT_NE(grass, sand);
    EXPECT_NE(grass, snow);
    EXPECT_NE(grass, water);
    EXPECT_NE(dirt, stone);
}

TEST(VoxelType, ColorsAreInUnitRange) {
    VoxelType types[] = {VoxelType::Grass, VoxelType::Dirt, VoxelType::Stone,
                         VoxelType::Sand, VoxelType::Snow, VoxelType::Water};
    for (auto type : types) {
        auto c = voxel_color(type);
        EXPECT_GE(c.x, 0.0f);
        EXPECT_LE(c.x, 1.0f);
        EXPECT_GE(c.y, 0.0f);
        EXPECT_LE(c.y, 1.0f);
        EXPECT_GE(c.z, 0.0f);
        EXPECT_LE(c.z, 1.0f);
    }
}

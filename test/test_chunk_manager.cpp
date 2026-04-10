#include <voxel/chunk_manager.hpp>

#include <gtest/gtest.h>

#include <unordered_set>

using namespace voxel;

// --- ChunkColumnKey ---

TEST(ChunkColumnKey, EqualityOperator) {
    ChunkColumnKey a{1, 2};
    ChunkColumnKey b{1, 2};
    ChunkColumnKey c{1, 3};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(ChunkColumnKey, HashDiffersForDifferentKeys) {
    ChunkColumnKeyHash hash;
    ChunkColumnKey a{0, 0};
    ChunkColumnKey b{1, 0};
    ChunkColumnKey c{0, 1};
    // Different keys should (very likely) have different hashes
    EXPECT_NE(hash(a), hash(b));
    EXPECT_NE(hash(a), hash(c));
    EXPECT_NE(hash(b), hash(c));
}

TEST(ChunkColumnKey, HashConsistency) {
    ChunkColumnKeyHash hash;
    ChunkColumnKey key{42, -7};
    EXPECT_EQ(hash(key), hash(key));
}

TEST(ChunkColumnKey, UsableInUnorderedSet) {
    std::unordered_set<ChunkColumnKey, ChunkColumnKeyHash> set;
    set.insert({0, 0});
    set.insert({1, 0});
    set.insert({0, 0});  // duplicate
    EXPECT_EQ(set.size(), 2u);
    EXPECT_TRUE(set.contains({0, 0}));
    EXPECT_TRUE(set.contains({1, 0}));
    EXPECT_FALSE(set.contains({2, 2}));
}

TEST(ChunkColumnKey, NegativeCoordinates) {
    ChunkColumnKey a{-1, -2};
    ChunkColumnKey b{-1, -2};
    EXPECT_EQ(a, b);

    ChunkColumnKeyHash hash;
    EXPECT_EQ(hash(a), hash(b));
}

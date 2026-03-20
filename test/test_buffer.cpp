#include <gtest/gtest.h>

#include <steel/buffer.hpp>
#include <vulkan/vulkan.hpp>

#include <cstddef>
#include <cstdint>

// --------------------------------------------------------------------------
// Buffer class type traits
// --------------------------------------------------------------------------

TEST(Buffer, IsMovable) {
    EXPECT_TRUE(std::is_move_constructible_v<steel::Buffer>);
    EXPECT_TRUE(std::is_move_assignable_v<steel::Buffer>);
}

TEST(Buffer, IsNotCopyable) {
    // Buffers own Vulkan resources and should not be copied.
    EXPECT_FALSE(std::is_copy_constructible_v<steel::Buffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<steel::Buffer>);
}

// --------------------------------------------------------------------------
// Vulkan buffer create-info construction tests
// --------------------------------------------------------------------------
// These verify that we can correctly populate Vulkan structs that the Buffer
// implementation uses internally. No GPU needed.

TEST(BufferCreateInfo, DefaultConstruction) {
    vk::BufferCreateInfo info{};
    EXPECT_EQ(info.size, 0u);
    EXPECT_EQ(info.usage, vk::BufferUsageFlags{});
    EXPECT_EQ(info.sharingMode, vk::SharingMode::eExclusive);
}

TEST(BufferCreateInfo, VertexBufferUsage) {
    vk::BufferCreateInfo info{};
    info.size  = 1024;
    info.usage = vk::BufferUsageFlagBits::eVertexBuffer;

    EXPECT_EQ(info.size, 1024u);
    EXPECT_TRUE(info.usage & vk::BufferUsageFlagBits::eVertexBuffer);
}

TEST(BufferCreateInfo, StagingBufferUsage) {
    vk::BufferCreateInfo info{};
    info.size  = 256;
    info.usage = vk::BufferUsageFlagBits::eTransferSrc;

    EXPECT_TRUE(info.usage & vk::BufferUsageFlagBits::eTransferSrc);
    EXPECT_FALSE(info.usage & vk::BufferUsageFlagBits::eVertexBuffer);
}

TEST(BufferCreateInfo, CombinedUsageFlags) {
    auto usage = vk::BufferUsageFlagBits::eVertexBuffer
               | vk::BufferUsageFlagBits::eTransferDst;

    EXPECT_TRUE(usage & vk::BufferUsageFlagBits::eVertexBuffer);
    EXPECT_TRUE(usage & vk::BufferUsageFlagBits::eTransferDst);
    EXPECT_FALSE(usage & vk::BufferUsageFlagBits::eUniformBuffer);
}

// --------------------------------------------------------------------------
// Memory property flags tests
// --------------------------------------------------------------------------

TEST(MemoryProperties, DeviceLocal) {
    auto props = vk::MemoryPropertyFlagBits::eDeviceLocal;
    EXPECT_TRUE(props & vk::MemoryPropertyFlagBits::eDeviceLocal);
    EXPECT_FALSE(props & vk::MemoryPropertyFlagBits::eHostVisible);
}

TEST(MemoryProperties, HostVisibleCoherent) {
    auto props = vk::MemoryPropertyFlagBits::eHostVisible
               | vk::MemoryPropertyFlagBits::eHostCoherent;

    EXPECT_TRUE(props & vk::MemoryPropertyFlagBits::eHostVisible);
    EXPECT_TRUE(props & vk::MemoryPropertyFlagBits::eHostCoherent);
    EXPECT_FALSE(props & vk::MemoryPropertyFlagBits::eDeviceLocal);
}

// --------------------------------------------------------------------------
// Buffer size calculation tests
// --------------------------------------------------------------------------
// Common patterns: vertex data sizing, alignment.

TEST(BufferSizing, VertexArraySize) {
    struct SimpleVertex {
        float position[3];
        float color[3];
    };

    constexpr size_t vertex_count = 100;
    constexpr size_t expected     = vertex_count * sizeof(SimpleVertex);

    EXPECT_EQ(expected, 2400u);  // 100 * 24 bytes
}

TEST(BufferSizing, UniformBufferAlignment) {
    // Vulkan requires uniform buffers to be aligned to
    // minUniformBufferOffsetAlignment, commonly 256 bytes.
    constexpr vk::DeviceSize alignment = 256;
    constexpr vk::DeviceSize data_size = 128;

    // Round up to alignment boundary.
    constexpr vk::DeviceSize aligned = (data_size + alignment - 1) & ~(alignment - 1);
    EXPECT_EQ(aligned, 256u);
}

TEST(BufferSizing, UniformBufferAlignmentOddSize) {
    constexpr vk::DeviceSize alignment = 256;
    constexpr vk::DeviceSize data_size = 300;

    constexpr vk::DeviceSize aligned = (data_size + alignment - 1) & ~(alignment - 1);
    EXPECT_EQ(aligned, 512u);
}

TEST(BufferSizing, ZeroSizeAligns) {
    constexpr vk::DeviceSize alignment = 256;
    constexpr vk::DeviceSize data_size = 0;

    constexpr vk::DeviceSize aligned = (data_size + alignment - 1) & ~(alignment - 1);
    EXPECT_EQ(aligned, 0u);
}

// --------------------------------------------------------------------------
// Memory allocate info struct tests
// --------------------------------------------------------------------------

TEST(MemoryAllocateInfo, Construction) {
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize  = 4096;
    alloc_info.memoryTypeIndex = 2;

    EXPECT_EQ(alloc_info.allocationSize, 4096u);
    EXPECT_EQ(alloc_info.memoryTypeIndex, 2u);
}

// --------------------------------------------------------------------------
// Span-to-byte conversion (used by create_vertex_buffer)
// --------------------------------------------------------------------------

TEST(SpanConversion, FloatArrayToBytes) {
    float data[] = {1.0f, 2.0f, 3.0f};
    auto byte_span = std::as_bytes(std::span{data});

    EXPECT_EQ(byte_span.size(), sizeof(data));
    EXPECT_EQ(byte_span.size(), 12u);
}

TEST(SpanConversion, VertexArrayToBytes) {
    struct V {
        float pos[3];
        float col[3];
    };

    V vertices[] = {
        {{0, 0, 0}, {1, 0, 0}},
        {{1, 0, 0}, {0, 1, 0}},
        {{0, 1, 0}, {0, 0, 1}},
    };

    auto byte_span = std::as_bytes(std::span{vertices});
    EXPECT_EQ(byte_span.size(), 3 * sizeof(V));
    EXPECT_EQ(byte_span.size(), 72u);
}

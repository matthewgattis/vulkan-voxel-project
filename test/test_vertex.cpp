#include <gtest/gtest.h>

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <cstddef>
#include <type_traits>

// A Vertex struct matching the voxel project's layout: position (vec3) + normal (vec3) + color (vec3).
// Defined here so tests don't depend on the voxel target.
namespace test {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;

    static auto binding_description() -> vk::VertexInputBindingDescription {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static auto attribute_descriptions()
        -> std::array<vk::VertexInputAttributeDescription, 3> {
        return {{
            {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)},
            {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)},
            {2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)},
        }};
    }
};

} // namespace test

// --------------------------------------------------------------------------
// Vertex struct layout tests
// --------------------------------------------------------------------------

TEST(Vertex, SizeIs36Bytes) {
    // vec3 (12 bytes) + vec3 (12 bytes) + vec3 (12 bytes) = 36 bytes, no padding expected.
    EXPECT_EQ(sizeof(test::Vertex), 36u);
}

TEST(Vertex, PositionOffsetIsZero) {
    EXPECT_EQ(offsetof(test::Vertex, position), 0u);
}

TEST(Vertex, NormalOffsetIs12) {
    EXPECT_EQ(offsetof(test::Vertex, normal), 12u);
}

TEST(Vertex, ColorOffsetIs24) {
    EXPECT_EQ(offsetof(test::Vertex, color), 24u);
}

TEST(Vertex, IsStandardLayout) {
    EXPECT_TRUE(std::is_standard_layout_v<test::Vertex>);
}

TEST(Vertex, IsTriviallyCopiable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<test::Vertex>);
}

// --------------------------------------------------------------------------
// Binding description tests
// --------------------------------------------------------------------------

TEST(Vertex, BindingDescriptionBindingIsZero) {
    auto binding = test::Vertex::binding_description();
    EXPECT_EQ(binding.binding, 0u);
}

TEST(Vertex, BindingDescriptionStrideMatchesSize) {
    auto binding = test::Vertex::binding_description();
    EXPECT_EQ(binding.stride, sizeof(test::Vertex));
}

TEST(Vertex, BindingDescriptionInputRateIsVertex) {
    auto binding = test::Vertex::binding_description();
    EXPECT_EQ(binding.inputRate, vk::VertexInputRate::eVertex);
}

// --------------------------------------------------------------------------
// Attribute description tests
// --------------------------------------------------------------------------

TEST(Vertex, HasThreeAttributeDescriptions) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs.size(), 3u);
}

TEST(Vertex, PositionAttributeFormat) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[0].format, vk::Format::eR32G32B32Sfloat);
}

TEST(Vertex, PositionAttributeOffset) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[0].offset, 0u);
}

TEST(Vertex, NormalAttributeFormat) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[1].format, vk::Format::eR32G32B32Sfloat);
}

TEST(Vertex, NormalAttributeOffset) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[1].offset, 12u);
}

TEST(Vertex, ColorAttributeFormat) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[2].format, vk::Format::eR32G32B32Sfloat);
}

TEST(Vertex, ColorAttributeOffset) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[2].offset, 24u);
}

TEST(Vertex, AttributeLocationsAreSequential) {
    auto attrs = test::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[0].location, 0u);
    EXPECT_EQ(attrs[1].location, 1u);
    EXPECT_EQ(attrs[2].location, 2u);
}

TEST(Vertex, AllAttributesUseSameBinding) {
    auto attrs = test::Vertex::attribute_descriptions();
    for (const auto& attr : attrs) {
        EXPECT_EQ(attr.binding, 0u);
    }
}

// --------------------------------------------------------------------------
// glm vec3 sanity checks (ensure our math library is configured correctly)
// --------------------------------------------------------------------------

TEST(GlmSanity, Vec3SizeIs12) {
    EXPECT_EQ(sizeof(glm::vec3), 12u);
}

TEST(GlmSanity, Vec3ComponentAccess) {
    glm::vec3 v{1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(GlmSanity, Mat4SizeIs64) {
    EXPECT_EQ(sizeof(glm::mat4), 64u);
}

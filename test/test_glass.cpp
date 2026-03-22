#include <gtest/gtest.h>

#include <glass/vertex.hpp>
#include <glass/mesh.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/shader.hpp>
#include <cstddef>
#include <type_traits>

#include <glm/glm.hpp>

// --------------------------------------------------------------------------
// glass::Vertex struct layout tests
// --------------------------------------------------------------------------

TEST(GlassVertex, SizeIs36Bytes) {
    EXPECT_EQ(sizeof(glass::Vertex), 36u);
}

TEST(GlassVertex, IsStandardLayout) {
    EXPECT_TRUE(std::is_standard_layout_v<glass::Vertex>);
}

TEST(GlassVertex, IsTriviallyCopiable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<glass::Vertex>);
}

TEST(GlassVertex, PositionOffsetIsZero) {
    EXPECT_EQ(offsetof(glass::Vertex, position), 0u);
}

TEST(GlassVertex, NormalOffsetIs12) {
    EXPECT_EQ(offsetof(glass::Vertex, normal), 12u);
}

TEST(GlassVertex, ColorOffsetIs24) {
    EXPECT_EQ(offsetof(glass::Vertex, color), 24u);
}

// --------------------------------------------------------------------------
// glass::Vertex binding description tests
// --------------------------------------------------------------------------

TEST(GlassVertex, BindingDescriptionStrideMatchesSize) {
    auto binding = glass::Vertex::binding_description();
    EXPECT_EQ(binding.stride, sizeof(glass::Vertex));
}

TEST(GlassVertex, BindingDescriptionInputRateIsVertex) {
    auto binding = glass::Vertex::binding_description();
    EXPECT_EQ(binding.inputRate, vk::VertexInputRate::eVertex);
}

TEST(GlassVertex, BindingDescriptionBindingIsZero) {
    auto binding = glass::Vertex::binding_description();
    EXPECT_EQ(binding.binding, 0u);
}

// --------------------------------------------------------------------------
// glass::Vertex attribute description tests
// --------------------------------------------------------------------------

TEST(GlassVertex, HasThreeAttributeDescriptions) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs.size(), 3u);
}

TEST(GlassVertex, PositionAttributeFormat) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[0].format, vk::Format::eR32G32B32Sfloat);
}

TEST(GlassVertex, PositionAttributeOffset) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[0].offset, 0u);
}

TEST(GlassVertex, PositionAttributeLocation) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[0].location, 0u);
}

TEST(GlassVertex, NormalAttributeFormat) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[1].format, vk::Format::eR32G32B32Sfloat);
}

TEST(GlassVertex, NormalAttributeOffset) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[1].offset, 12u);
}

TEST(GlassVertex, NormalAttributeLocation) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[1].location, 1u);
}

TEST(GlassVertex, ColorAttributeFormat) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[2].format, vk::Format::eR32G32B32Sfloat);
}

TEST(GlassVertex, ColorAttributeOffset) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[2].offset, 24u);
}

TEST(GlassVertex, ColorAttributeLocation) {
    auto attrs = glass::Vertex::attribute_descriptions();
    EXPECT_EQ(attrs[2].location, 2u);
}

TEST(GlassVertex, AllAttributesUseSameBinding) {
    auto attrs = glass::Vertex::attribute_descriptions();
    for (const auto& attr : attrs) {
        EXPECT_EQ(attr.binding, 0u);
    }
}

// --------------------------------------------------------------------------
// glass::Mesh interface tests
// --------------------------------------------------------------------------

TEST(GlassMesh, MeshIsAbstract) {
    EXPECT_TRUE(std::is_abstract_v<glass::Mesh>);
}

TEST(GlassMesh, MeshHasVirtualDestructor) {
    EXPECT_TRUE(std::has_virtual_destructor_v<glass::Mesh>);
}

// --------------------------------------------------------------------------
// glass::Shader tests
// --------------------------------------------------------------------------

TEST(GlassShader, IsMoveConstructible) {
    EXPECT_TRUE(std::is_move_constructible_v<glass::Shader>);
}

// --------------------------------------------------------------------------
// glass::Material tests
// --------------------------------------------------------------------------

TEST(GlassMaterial, IsMoveConstructible) {
    EXPECT_TRUE(std::is_move_constructible_v<glass::Material>);
}

TEST(GlassMaterial, IsNotCopyConstructible) {
    EXPECT_FALSE(std::is_copy_constructible_v<glass::Material>);
}

TEST(GlassMaterial, IsNotCopyAssignable) {
    EXPECT_FALSE(std::is_copy_assignable_v<glass::Material>);
}

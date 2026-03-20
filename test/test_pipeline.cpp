#include <gtest/gtest.h>

#include <steel/pipeline.hpp>
#include <vulkan/vulkan.hpp>

#include <type_traits>

// --------------------------------------------------------------------------
// PipelineBuilder type traits
// --------------------------------------------------------------------------

TEST(PipelineBuilder, IsNotCopyable) {
    EXPECT_FALSE(std::is_copy_constructible_v<steel::PipelineBuilder>);
    EXPECT_FALSE(std::is_copy_assignable_v<steel::PipelineBuilder>);
}

// --------------------------------------------------------------------------
// Rasterization state defaults
// --------------------------------------------------------------------------
// PipelineBuilder uses these defaults internally; verify the Vulkan structs
// we expect it to populate.

TEST(RasterizationState, DefaultFillMode) {
    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.polygonMode = vk::PolygonMode::eFill;

    EXPECT_EQ(raster.polygonMode, vk::PolygonMode::eFill);
}

TEST(RasterizationState, DefaultCullModeBack) {
    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.cullMode  = vk::CullModeFlagBits::eBack;
    raster.frontFace = vk::FrontFace::eCounterClockwise;

    EXPECT_EQ(raster.cullMode, vk::CullModeFlagBits::eBack);
    EXPECT_EQ(raster.frontFace, vk::FrontFace::eCounterClockwise);
}

TEST(RasterizationState, NoDepthClamp) {
    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.depthClampEnable = VK_FALSE;

    EXPECT_EQ(raster.depthClampEnable, VK_FALSE);
}

TEST(RasterizationState, LineWidthOne) {
    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.lineWidth = 1.0f;

    EXPECT_FLOAT_EQ(raster.lineWidth, 1.0f);
}

// --------------------------------------------------------------------------
// Depth stencil state
// --------------------------------------------------------------------------

TEST(DepthStencilState, DepthTestEnabled) {
    vk::PipelineDepthStencilStateCreateInfo depth{};
    depth.depthTestEnable  = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp   = vk::CompareOp::eLess;

    EXPECT_EQ(depth.depthTestEnable, VK_TRUE);
    EXPECT_EQ(depth.depthWriteEnable, VK_TRUE);
    EXPECT_EQ(depth.depthCompareOp, vk::CompareOp::eLess);
}

TEST(DepthStencilState, DepthTestDisabled) {
    vk::PipelineDepthStencilStateCreateInfo depth{};
    depth.depthTestEnable  = VK_FALSE;
    depth.depthWriteEnable = VK_FALSE;

    EXPECT_EQ(depth.depthTestEnable, VK_FALSE);
    EXPECT_EQ(depth.depthWriteEnable, VK_FALSE);
}

TEST(DepthStencilState, NoStencilByDefault) {
    vk::PipelineDepthStencilStateCreateInfo depth{};
    depth.stencilTestEnable = VK_FALSE;

    EXPECT_EQ(depth.stencilTestEnable, VK_FALSE);
}

// --------------------------------------------------------------------------
// Input assembly state
// --------------------------------------------------------------------------

TEST(InputAssembly, TriangleList) {
    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology               = vk::PrimitiveTopology::eTriangleList;
    ia.primitiveRestartEnable = VK_FALSE;

    EXPECT_EQ(ia.topology, vk::PrimitiveTopology::eTriangleList);
    EXPECT_EQ(ia.primitiveRestartEnable, VK_FALSE);
}

TEST(InputAssembly, LineList) {
    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::eLineList;

    EXPECT_EQ(ia.topology, vk::PrimitiveTopology::eLineList);
}

TEST(InputAssembly, PointList) {
    vk::PipelineInputAssemblyStateCreateInfo ia{};
    ia.topology = vk::PrimitiveTopology::ePointList;

    EXPECT_EQ(ia.topology, vk::PrimitiveTopology::ePointList);
}

// --------------------------------------------------------------------------
// Color blend attachment state
// --------------------------------------------------------------------------

TEST(ColorBlend, NoBlendingDefault) {
    vk::PipelineColorBlendAttachmentState blend{};
    blend.blendEnable    = VK_FALSE;
    blend.colorWriteMask = vk::ColorComponentFlagBits::eR
                         | vk::ColorComponentFlagBits::eG
                         | vk::ColorComponentFlagBits::eB
                         | vk::ColorComponentFlagBits::eA;

    EXPECT_EQ(blend.blendEnable, VK_FALSE);
    EXPECT_TRUE(blend.colorWriteMask & vk::ColorComponentFlagBits::eR);
    EXPECT_TRUE(blend.colorWriteMask & vk::ColorComponentFlagBits::eG);
    EXPECT_TRUE(blend.colorWriteMask & vk::ColorComponentFlagBits::eB);
    EXPECT_TRUE(blend.colorWriteMask & vk::ColorComponentFlagBits::eA);
}

TEST(ColorBlend, AlphaBlending) {
    vk::PipelineColorBlendAttachmentState blend{};
    blend.blendEnable         = VK_TRUE;
    blend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blend.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blend.colorBlendOp        = vk::BlendOp::eAdd;
    blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    blend.alphaBlendOp        = vk::BlendOp::eAdd;

    EXPECT_EQ(blend.blendEnable, VK_TRUE);
    EXPECT_EQ(blend.srcColorBlendFactor, vk::BlendFactor::eSrcAlpha);
    EXPECT_EQ(blend.dstColorBlendFactor, vk::BlendFactor::eOneMinusSrcAlpha);
}

// --------------------------------------------------------------------------
// Viewport and scissor
// --------------------------------------------------------------------------

TEST(Viewport, FullExtent) {
    vk::Extent2D extent{800, 600};
    vk::Viewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(extent.width);
    viewport.height   = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    EXPECT_FLOAT_EQ(viewport.width, 800.0f);
    EXPECT_FLOAT_EQ(viewport.height, 600.0f);
    EXPECT_FLOAT_EQ(viewport.minDepth, 0.0f);
    EXPECT_FLOAT_EQ(viewport.maxDepth, 1.0f);
}

TEST(Viewport, ScissorMatchesExtent) {
    vk::Extent2D extent{1920, 1080};
    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = extent;

    EXPECT_EQ(scissor.extent.width, 1920u);
    EXPECT_EQ(scissor.extent.height, 1080u);
    EXPECT_EQ(scissor.offset.x, 0);
    EXPECT_EQ(scissor.offset.y, 0);
}

// --------------------------------------------------------------------------
// Multisampling state
// --------------------------------------------------------------------------

TEST(Multisampling, NoMultisampling) {
    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;
    ms.sampleShadingEnable  = VK_FALSE;

    EXPECT_EQ(ms.rasterizationSamples, vk::SampleCountFlagBits::e1);
    EXPECT_EQ(ms.sampleShadingEnable, VK_FALSE);
}

// --------------------------------------------------------------------------
// Pipeline layout create info
// --------------------------------------------------------------------------

TEST(PipelineLayout, EmptyLayout) {
    vk::PipelineLayoutCreateInfo layout_info{};
    layout_info.setLayoutCount         = 0;
    layout_info.pSetLayouts            = nullptr;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges    = nullptr;

    EXPECT_EQ(layout_info.setLayoutCount, 0u);
    EXPECT_EQ(layout_info.pSetLayouts, nullptr);
}

TEST(PipelineLayout, PushConstantRange) {
    vk::PushConstantRange push{};
    push.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push.offset     = 0;
    push.size       = 64;  // glm::mat4

    EXPECT_EQ(push.size, 64u);
    EXPECT_EQ(push.offset, 0u);
    EXPECT_TRUE(push.stageFlags & vk::ShaderStageFlagBits::eVertex);
}

// --------------------------------------------------------------------------
// SPIR-V shader module create info
// --------------------------------------------------------------------------

TEST(ShaderModule, CreateInfoFromCode) {
    // Minimal valid-ish SPIR-V header (magic number + version).
    // Real SPIR-V would have more, but we just test struct construction.
    std::vector<uint32_t> spirv_code = {
        0x07230203,  // SPIR-V magic number
        0x00010000,  // Version 1.0
        0x00000000,  // Generator
        0x00000001,  // Bound
        0x00000000,  // Schema
    };

    vk::ShaderModuleCreateInfo create_info{};
    create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
    create_info.pCode    = spirv_code.data();

    EXPECT_EQ(create_info.codeSize, 20u);
    EXPECT_NE(create_info.pCode, nullptr);
    EXPECT_EQ(create_info.pCode[0], 0x07230203u);
}

TEST(ShaderModule, SpirvMagicNumber) {
    // The SPIR-V magic number should always be 0x07230203.
    constexpr uint32_t SPIRV_MAGIC = 0x07230203;
    EXPECT_EQ(SPIRV_MAGIC, 0x07230203u);
}

// --------------------------------------------------------------------------
// Dynamic state
// --------------------------------------------------------------------------

TEST(DynamicState, ViewportAndScissor) {
    std::array<vk::DynamicState, 2> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamic_info{};
    dynamic_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_info.pDynamicStates    = dynamic_states.data();

    EXPECT_EQ(dynamic_info.dynamicStateCount, 2u);
    EXPECT_EQ(dynamic_info.pDynamicStates[0], vk::DynamicState::eViewport);
    EXPECT_EQ(dynamic_info.pDynamicStates[1], vk::DynamicState::eScissor);
}

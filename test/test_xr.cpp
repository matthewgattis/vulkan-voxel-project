#include <gtest/gtest.h>

#include <steel/engine.hpp>
#include <steel/xr_system.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <type_traits>

// --------------------------------------------------------------------------
// EngineConfig tests
// --------------------------------------------------------------------------

TEST(EngineConfig, DefaultConstruction) {
    steel::EngineConfig config{.title = "Test"};
    EXPECT_EQ(config.title, "Test");
    EXPECT_TRUE(config.extra_instance_extensions.empty());
    EXPECT_TRUE(config.extra_device_extensions.empty());
    EXPECT_FALSE(config.physical_device_query); // null by default
}

TEST(EngineConfig, WithExtensions) {
    steel::EngineConfig config{
        .title = "Test",
        .extra_instance_extensions = {"VK_KHR_get_physical_device_properties2"},
        .extra_device_extensions = {"VK_KHR_external_memory"},
    };
    EXPECT_EQ(config.extra_instance_extensions.size(), 1u);
    EXPECT_EQ(config.extra_device_extensions.size(), 1u);
}

TEST(EngineConfig, StringOwnership) {
    // Verify EngineConfig owns its extension strings (no dangling pointers)
    steel::EngineConfig config{.title = "Test"};
    {
        std::string ext = "VK_KHR_test_extension";
        config.extra_instance_extensions.push_back(ext);
    }
    // ext is destroyed, but config should still have a valid copy
    EXPECT_EQ(config.extra_instance_extensions[0], "VK_KHR_test_extension");
}

// --------------------------------------------------------------------------
// XrSystem type traits
// --------------------------------------------------------------------------

TEST(XrSystem, IsNotCopyable) {
    EXPECT_FALSE(std::is_copy_constructible_v<steel::XrSystem>);
    EXPECT_FALSE(std::is_copy_assignable_v<steel::XrSystem>);
}

TEST(XrSystem, IsNotMovable) {
    EXPECT_FALSE(std::is_move_constructible_v<steel::XrSystem>);
    EXPECT_FALSE(std::is_move_assignable_v<steel::XrSystem>);
}

// --------------------------------------------------------------------------
// XrSystem::query_requirements graceful fallback
// --------------------------------------------------------------------------

TEST(XrSystem, QueryRequirementsReturnsNulloptOrValid) {
    // This test runs in CI and dev environments. It should not crash
    // regardless of whether an OpenXR runtime is installed.
    auto reqs = steel::XrSystem::query_requirements();

    if (reqs.has_value()) {
        // If an HMD is available, requirements should be valid
        // (we can't assert much more without GPU access)
        SUCCEED() << "OpenXR runtime found";
    } else {
        // Graceful fallback: no crash, no throw
        SUCCEED() << "No OpenXR runtime, desktop-only mode";
    }
}

// --------------------------------------------------------------------------
// XrVulkanRequirements
// --------------------------------------------------------------------------

TEST(XrVulkanRequirements, DefaultConstruction) {
    steel::XrVulkanRequirements reqs{};
    EXPECT_TRUE(reqs.instance_extensions.empty());
    EXPECT_TRUE(reqs.device_extensions.empty());
    EXPECT_EQ(reqs.max_vulkan_api_version, 0u);
}

// --------------------------------------------------------------------------
// XrFrameState
// --------------------------------------------------------------------------

TEST(XrFrameState, DefaultState) {
    steel::XrFrameState state{};
    EXPECT_FALSE(state.should_render);
    EXPECT_EQ(state.predicted_display_time, 0);
}

TEST(XrFrameState, EyeViewDefaultIsIdentity) {
    steel::XrFrameState state{};
    for (int eye = 0; eye < 2; ++eye) {
        EXPECT_EQ(state.eyes[eye].view, glm::mat4(1.0f));
        EXPECT_EQ(state.eyes[eye].projection, glm::mat4(1.0f));
    }
}

// --------------------------------------------------------------------------
// Coordinate transform math (XR Y-up → engine Z-up)
//
// The XR-to-engine transform is: rotate -90° around X.
// XR: X=right, Y=up, Z=back
// Engine: X=right, Y=forward, Z=up
//
// So:
//   XR Y-up  → Engine Z-up
//   XR Z-back → Engine Y-back (i.e. -Z in XR maps to -Y in engine... wait)
//
// The rotation matrix R(-90°, X):
//   [1  0    0  ]     XR(1,0,0) → Engine(1,0,0)  ✓ X=right
//   [0  0    1  ]     XR(0,1,0) → Engine(0,0,1)  ✓ Y-up → Z-up
//   [0 -1    0  ]     XR(0,0,1) → Engine(0,-1,0) ✓ Z-back → Y-back (engine forward is +Y)
// --------------------------------------------------------------------------

TEST(XrCoordinateTransform, YUpToZUp) {
    // The transform used in XrSystem: rotate(+pi/2, X-axis)
    // This maps: XR Y-up → engine Z-up, XR Z-back → engine Y-forward-negated
    //   (1,0,0) → (1,0,0)   X stays X
    //   (0,1,0) → (0,0,1)   Y-up → Z-up
    //   (0,0,1) → (0,-1,0)  Z-back → -Y (engine forward is +Y, XR forward is -Z)
    static const glm::mat4 xr_to_engine =
        glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));

    // XR right (1,0,0) → engine right (1,0,0)
    glm::vec4 xr_right = xr_to_engine * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_NEAR(xr_right.x, 1.0f, 1e-5f);
    EXPECT_NEAR(xr_right.y, 0.0f, 1e-5f);
    EXPECT_NEAR(xr_right.z, 0.0f, 1e-5f);

    // XR up (0,1,0) → engine up (0,0,1)
    glm::vec4 xr_up = xr_to_engine * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
    EXPECT_NEAR(xr_up.x, 0.0f, 1e-5f);
    EXPECT_NEAR(xr_up.y, 0.0f, 1e-5f);
    EXPECT_NEAR(xr_up.z, 1.0f, 1e-5f);

    // XR forward (0,0,-1) → engine forward (0,1,0)
    glm::vec4 xr_forward = xr_to_engine * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    EXPECT_NEAR(xr_forward.x, 0.0f, 1e-5f);
    EXPECT_NEAR(xr_forward.y, 1.0f, 1e-5f);
    EXPECT_NEAR(xr_forward.z, 0.0f, 1e-5f);
}

TEST(XrProjection, AsymmetricFrustumProducesValidMatrix) {
    // Simulate typical XR FOV angles (symmetric for simplicity)
    float near = 0.1f;
    float far = 500.0f;
    float half_fov = glm::radians(45.0f);

    float left   = near * std::tan(-half_fov);
    float right  = near * std::tan(half_fov);
    float up     = near * std::tan(half_fov);
    float down   = near * std::tan(-half_fov);

    glm::mat4 proj = glm::frustum(left, right, down, up, near, far);
    proj[1][1] *= -1.0f; // Vulkan Y-flip
    proj[2][1] *= -1.0f;

    // The projection should not be identity
    EXPECT_NE(proj, glm::mat4(1.0f));

    // [0][0] and [1][1] should be non-zero (focal lengths)
    EXPECT_NE(proj[0][0], 0.0f);
    EXPECT_NE(proj[1][1], 0.0f);

    // Vulkan Y-flip: [1][1] should be negative (flipped from OpenGL convention)
    EXPECT_LT(proj[1][1], 0.0f);

    // [3][2] should be non-zero (perspective divide)
    EXPECT_NE(proj[3][2], 0.0f);

    // Symmetric frustum: [2][1] should still be 0 (no asymmetric offset)
    EXPECT_NEAR(proj[2][1], 0.0f, 1e-6f);
}

TEST(XrProjection, AsymmetricYFlipNegatesOffsetTerm) {
    // Real XR FOV values (asymmetric, from Meta Quest)
    float near = 0.1f;
    float far = 500.0f;
    float left   = near * std::tan(glm::radians(-54.0f));
    float right  = near * std::tan(glm::radians(40.0f));
    float up     = near * std::tan(glm::radians(44.0f));
    float down   = near * std::tan(glm::radians(-55.0f));

    glm::mat4 proj = glm::frustum(left, right, down, up, near, far);

    // Before Y-flip: [2][1] should be non-zero for asymmetric frustum
    float offset_before = proj[2][1];
    EXPECT_NE(offset_before, 0.0f);

    // Apply Vulkan Y-flip (must negate both scale and offset)
    proj[1][1] *= -1.0f;
    proj[2][1] *= -1.0f;

    // After flip: [1][1] negative, [2][1] sign reversed
    EXPECT_LT(proj[1][1], 0.0f);
    EXPECT_NEAR(proj[2][1], -offset_before, 1e-6f);
}

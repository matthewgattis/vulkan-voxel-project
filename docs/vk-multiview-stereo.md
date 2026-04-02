# VK_KHR_multiview for Stereo Rendering

## Overview

`VK_KHR_multiview` (core in Vulkan 1.1+) allows rendering multiple views in a single render pass using layered framebuffer attachments. The shader receives `gl_ViewIndex` to select per-view data (e.g., left/right eye matrices).

## Current approach (two-pass)

The initial OpenXR implementation renders each eye as a separate render pass:
- Simple: reuses the existing single-view pipeline, shaders, and framebuffers unchanged
- Each eye gets its own UBO update + render pass begin/end + draw calls
- Doubles the CPU-side draw call overhead (not GPU draw calls themselves, since each pass is independent)

## Multiview alternative

With multiview, both eyes render in a **single render pass**:

### Render pass changes
```cpp
VkRenderPassMultiviewCreateInfo multiview_info{};
uint32_t view_mask = 0b11;        // views 0 and 1
uint32_t correlation_mask = 0b11; // views are spatially correlated
multiview_info.subpassCount = 1;
multiview_info.pViewMasks = &view_mask;
multiview_info.correlationMaskCount = 1;
multiview_info.pCorrelationMasks = &correlation_mask;
// Chain into VkRenderPassCreateInfo::pNext
```

### Framebuffer changes
- Color and depth attachments become `VkImageView` with `viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY` and `layerCount = 2`
- One layer per eye

### Shader changes (triangle.vert)
```glsl
#extension GL_EXT_multiview : require

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view[2];
    mat4 projection[2];
} frame;

// In main():
mat4 view = frame.view[gl_ViewIndex];
mat4 proj = frame.projection[gl_ViewIndex];
```

### UBO changes
```cpp
struct StereoFrameUBO {
    glm::mat4 view[2];        // 128 bytes
    glm::mat4 projection[2];  // 128 bytes
};                              // Total: 256 bytes
```

### FXAA considerations
- FXAA post-process would need to operate per-layer or use separate passes per eye
- Could apply FXAA only to the desktop companion and skip it for VR (many HMDs have their own anti-aliasing or the pixel density makes FXAA less impactful)

## Trade-offs

| Aspect | Two-pass | Multiview |
|--------|----------|-----------|
| CPU draw call overhead | 2x (each eye re-records all draws) | 1x (single pass) |
| GPU efficiency | GPU handles each pass independently | GPU may batch/parallelize across views |
| Shader changes | None | Need `gl_ViewIndex`, array UBO |
| Render pass changes | None | Multiview create info, layered attachments |
| Framebuffer changes | None | 2D array images |
| Debugging | Easier (standard render passes) | Harder (layered output) |
| Implementation risk | Low | Medium |
| Compatibility | Universal | Vulkan 1.1+ (we target 1.3, so fine) |

## When to switch

The two-pass approach is correct for the initial implementation. Consider switching to multiview when:
- Draw call count is high enough that CPU overhead matters (thousands of chunks visible)
- Profiling shows the two-pass approach is GPU-bottlenecked on redundant vertex processing
- The shader pipeline stabilizes (adding multiview to a changing shader is extra churn)

## References
- [Vulkan spec: VK_KHR_multiview](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_multiview.html)
- [OpenXR spec: composition layer projection](https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#compositing)

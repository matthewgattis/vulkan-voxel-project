# Vulkan Render Passes, Subpasses, and Tile-Based GPUs

## Render passes as image layout boundaries

A Vulkan render pass defines a scope in which you draw into attachments (color, depth). When the render pass ends, the attachments transition to a final image layout (e.g., `SHADER_READ_ONLY_OPTIMAL`). This transition is the mechanism that makes an attachment available as a sampled texture for subsequent passes.

In this engine, the desktop pipeline has three render passes:

1. **Scene pass** — forward-renders geometry into an offscreen color target. Final layout: `SHADER_READ_ONLY_OPTIMAL`.
2. **FXAA pass** — a single full-screen triangle reads the offscreen image as a texture, runs the FXAA shader, writes to the swapchain image.
3. **ImGui pass** — composites the UI on top of the anti-aliased image, so UI text isn't blurred by FXAA.

Each "pass" is really just one or a few draw calls. The render pass boundaries exist because you can't read from an attachment you're currently writing to within the same render pass — that's a feedback loop.

## Tile-based vs immediate-mode GPUs

### Immediate-mode (desktop GPUs like the RTX 4090)

Processes triangles as they arrive. Each fragment shader reads and writes directly to color/depth buffers in VRAM. Memory bandwidth is paid on every access, but desktop GPUs have enormous VRAM bandwidth.

### Tile-based (mobile GPUs, Quest native renderer)

Divides the framebuffer into small tiles (e.g., 32x32 pixels). For each tile:

1. Bins all triangles that touch the tile
2. Loads the tile into small, fast **on-chip tile memory** (a few KB, on the GPU die itself)
3. Renders all triangles for that tile — all reads/writes hit tile memory, not VRAM
4. When the render pass ends, stores the finished tile out to VRAM

Tile memory is much faster and more power-efficient than VRAM. The architecture minimizes VRAM traffic, which is critical for mobile power budgets.

## The cost of separate render passes on tile-based GPUs

With two separate render passes (e.g., scene then FXAA):

```
Render Pass 1 (scene):
  For each tile:
    Clear/load tile memory
    Render scene geometry
    Store tile → VRAM                    ← full image written out

Render Pass 2 (FXAA):
  For each tile:
    Load tile ← VRAM                    ← full image read back in
    Run FXAA
    Store tile → VRAM                    ← result written out
```

The scene image makes a full round trip through VRAM between passes. At 2064x2272 RGBA8 per eye, that's ~19MB written then ~19MB read. On mobile, this bandwidth costs time and battery.

## Subpasses: staying in tile memory

Subpasses allow multiple drawing phases within a single render pass. The key optimization: data stays in tile memory between subpasses and never hits VRAM until the final store.

```
Render Pass (two subpasses):
  For each tile:
    Clear/load tile memory
    Subpass 0: Render scene → tile memory
    — subpass transition (no VRAM traffic) —
    Subpass 1: Read tile memory as input attachment, write result to tile memory
    Store tile → VRAM                    ← only the final result leaves the chip
```

The ~19MB round trip is eliminated entirely.

## Input attachments and their constraints

Within a subpass, you read the previous subpass's output via an **input attachment** rather than a sampled texture. The critical constraint: an input attachment can only read the value at the current fragment's position (`gl_FragCoord`). You cannot sample at an offset.

This constraint exists because tile memory only holds one tile at a time. When the GPU processes tile (3, 5), it has those 32x32 pixels loaded. The neighboring tile's pixels are not available. Reading `gl_FragCoord + vec2(1, 0)` could cross a tile boundary into unloaded memory.

### What fits in a subpass

Any effect that only needs the current pixel: grayscale conversion, tone mapping, color grading, simple compositing, alpha blending overlays.

### What requires a separate render pass

Any effect that samples neighboring pixels: FXAA (contrast detection across adjacent pixels), blur, bloom, edge detection. These need the full image available in VRAM as a sampled texture so the sampler can access any pixel.

## What happens on desktop GPUs

Immediate-mode GPUs don't have tile memory. The driver sees subpasses and executes them as sequential draw calls with pipeline barriers between them. Performance is essentially the same as separate render passes.

Vulkan's subpass system was designed primarily for tile-based GPUs. Desktop GPUs support the API but don't benefit from the tile memory optimization. The code is portable across both architectures — it just only provides a speedup on tile-based hardware.

## Code shape

Separate render passes:

```cpp
vkCmdBeginRenderPass(cmd, &scenePassInfo, ...);
  // draw scene geometry
vkCmdEndRenderPass(cmd);

vkCmdBeginRenderPass(cmd, &fxaaPassInfo, ...);
  // draw full-screen triangle (samples scene as texture)
vkCmdEndRenderPass(cmd);
```

Subpasses within one render pass:

```cpp
vkCmdBeginRenderPass(cmd, &combinedPassInfo, ...);
  // Subpass 0: draw scene geometry → writes color attachment
vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
  // Subpass 1: reads color as input attachment → writes to output
vkCmdEndRenderPass(cmd);
```

The render pass creation declares both subpasses, their attachment references, and the dependency between them upfront. This gives the tile-based driver the full picture of data flow so it can schedule tile processing optimally.

## Relevance to this project

The desktop pipeline uses separate render passes (scene → FXAA → ImGui) because FXAA requires neighbor pixel access. This is correct and performs well on the RTX 4090.

If per-pixel post-processing effects are added in the future (tone mapping, color grading, vignette), these could be merged as subpasses rather than additional render passes. On desktop hardware the performance difference is negligible, but it keeps the architecture clean and portable to tile-based GPUs if the engine ever targets standalone Quest rendering.

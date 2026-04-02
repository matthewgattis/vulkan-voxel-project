# VR Anti-Aliasing Strategies

## Why desktop AA doesn't translate directly to VR

In VR the display is inches from your eyes, filling your field of view. Small visual artifacts that are invisible on a monitor become noticeable. Additionally, the head is always in motion — even "holding still" produces micro-movement from tracking. This constant motion interacts poorly with screen-space AA techniques.

## FXAA in VR

FXAA is a screen-space post-process: it reads the final image, detects edges via contrast, and blurs across them. It's nearly free (~0.5ms) and works well on desktop.

In VR, because head tracking shifts geometry across the pixel grid every frame, FXAA recomputes a different blur pattern each frame. The result is visible shimmer — the smoothing itself flickers as edges shift by sub-pixel amounts. The effect is subtle on desktop but pronounced in a headset.

This is analogous to a thermostat without hysteresis: a system that reacts purely to current state without memory of previous states oscillates at boundaries.

## MSAA (Multi-Sample Anti-Aliasing)

MSAA is the preferred AA approach for forward-rendered VR.

### How it works

Each pixel has N sample points at sub-pixel positions (4x MSAA = 4 samples). For each triangle rasterized:

1. **Coverage test** — which sample points does the triangle cover? Simple geometric point-in-triangle test per sample.
2. **Fragment shader** — runs **once** per pixel (not per sample), producing a single color.
3. **Sample storage** — covered samples receive the shaded color. Uncovered samples keep their previous value.
4. **Resolve** — after all geometry is drawn, samples are averaged to produce the final single-sample image.

Interior pixels (fully covered) are unaffected — all samples get the same color. Edge pixels get a blend proportional to coverage (e.g., 3/4 samples covered = 75% triangle color). This produces smooth, geometrically correct edges.

Each sample also has its own depth value, so MSAA correctly handles overlapping geometry at sub-pixel precision.

### Why it's stable in VR

MSAA operates at the geometry level, not the pixel level. The anti-aliasing is locked to triangle edges and moves with the scene. There's no per-frame recomputation from the image — the coverage ratio at each pixel is a direct geometric measurement. Head movement shifts which pixels are edge pixels, but each edge pixel's blend is computed correctly from scratch based on geometry, not from a heuristic pattern match on pixel colors.

### Performance cost

| Resource | Without MSAA | 4x MSAA |
|----------|-------------|---------|
| Color buffer (per eye, 2064x2272) | ~19 MB | ~75 MB |
| Depth buffer (per eye) | ~19 MB | ~75 MB |
| Fragment shader invocations | 1x | 1x |
| Coverage tests | 1x | 4x |
| Extra pass | none | resolve |

The memory and bandwidth cost is real but the shading cost is unchanged. On modern GPUs the coverage tests and resolve are cheap.

### MSAA vs SSAA

SSAA (supersampling) renders at higher resolution and downsamples. 4x SSAA runs the fragment shader 4x per pixel — 4x the shading cost. Both use the same buffer memory, but MSAA avoids the shading cost by running the fragment shader once and replicating the result to covered samples.

The tradeoff: MSAA only smooths geometry edges. Texture aliasing and shading aliasing (specular highlights, shadow edges) are unaffected because there's no variation across samples within a fully-covered pixel. SSAA smooths everything. For a voxel renderer with simple shading, MSAA's limitation is a non-issue.

### Sample patterns

GPUs use a rotated grid pattern rather than a regular 2x2 grid. A regular grid would miss nearly-horizontal or nearly-vertical edges (all samples on the same side of the edge). The rotated pattern ensures coverage variation for edges at any angle.

### Limitation

MSAA only anti-aliases geometry edges — the boundaries where triangles start or stop covering a pixel. It does not help with:

- Texture aliasing (solved by mipmapping)
- Shader aliasing (specular flicker, shadow edge stair-stepping)
- Alpha-tested edges (grass, foliage — though alpha-to-coverage can help)

## Deferred rendering and VR AA

Deferred rendering and MSAA are a poor fit:

- The G-buffer (normals, albedo, depth, roughness) is already multiple textures per pixel. 4x MSAA means 4x the G-buffer memory and bandwidth.
- Lighting passes would need to shade per-sample at edge pixels to avoid artifacts, partially defeating MSAA's "shade once" advantage.

Common strategies for deferred + VR:

| Approach | Tradeoff |
|----------|----------|
| TAA with aggressive clamping | Reduces ghosting during head movement at the cost of AA quality |
| Forward MSAA pass on top | Hybrid: deferred for lighting, forward+MSAA for edges. Complex |
| SMAA (morphological) | Smarter FXAA, but still screen-space — still shimmers in VR |
| Switch to forward rendering | Many VR titles do this (e.g., Half-Life: Alyx). Simpler lighting is acceptable when VR's immersion compensates |

The tension: deferred pushes toward temporal solutions, VR pushes away from them. This is a major reason many VR renderers use forward+MSAA.

## Current state in this project

The engine uses a forward renderer with FXAA on the desktop companion view. The XR eye renders have no AA. Since the renderer is already forward, adding MSAA to the XR path is straightforward:

1. Create XR swapchains with multi-sample images (or create separate MSAA render targets and resolve into the XR swapchain)
2. Create multi-sample depth buffers
3. Update the XR render pass with MSAA sample count and resolve attachments
4. The existing pipeline needs a variant with the matching sample count

Variable Rate Shading (VRS) is a complementary optimization: shade at lower rates in the lens periphery where distortion already blurs the image, saving budget for MSAA in the center. The RTX 4090 supports this.

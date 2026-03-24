# Architecture Review

_Reviewed by Cleo, 2026-03-24_

---

## Overall

The code is well-architected — not the typical single-file blob. The layered design (`steel` engine → `glass` rendering layer → `voxel` game logic) is clean and the separation of concerns is respected throughout.

---

## What's Working Well

### ChunkManager Threading

The most impressive part. The producer/consumer design is correct:

- `std::jthread` with `std::stop_token` + `stop_callback` for clean shutdown — the modern C++20 pattern done right
- Workers declared last in the struct so they're destroyed first (RAII ordering)
- Frustum-priority queue (`in_frustum` first, then distance) is a smart touch that reduces pop-in

### Meshing

Thorough for a WIP:

- Face culling crosses chunk boundaries via world-space queries (no seam cracks)
- Per-vertex ambient occlusion with the AO diagonal flip to avoid triangulation artifacts — most voxel tutorials skip this
- FNV-1a vertex deduplication hash is appropriate and fast

### Terrain Generation

- 6-octave fBm with a power curve above sea level for more dramatic peaks
- `TerrainColumn` precomputes heights and slopes once per column rather than requerying per-voxel — right approach for meshing throughput
- Snow coverage varying by slope gradient is a nice detail

### Shader

- Half-Lambert squared instead of raw N·L — softer shadows, better for voxel art
- Gaussian fog via `exp(-f²)` instead of linear falloff — subtle but looks better

---

## Issues & Improvements

### ChunkManager: Request Queue Rebuilt Every Frame

`ChunkManager::update` loops over the full 64×64 (LOAD_RADIUS=32) grid every frame and re-enqueues every not-yet-loaded column. This is a hot path that grows quadratically with radius.

**Fix:** Track which columns were in range last frame; only enqueue newly-entered columns.

### Terrain Queries in AO Sampling Hit Raw fBm

The worker uses `generator_.is_opaque_at` / `is_solid_at` for cross-chunk AO neighbor lookups. These call `height_at()`, which runs the full 6-octave fBm every call. `TerrainColumn` pre-caches heights correctly for the primary chunk, but AO border samples bypass this cache.

**Fix:** Expand the `TerrainColumn` cache by one voxel in each direction, or accept the cost for now (it's a border-only issue).

### Hardcoded Z Range in Frustum Culling

`column_in_frustum` uses `HEIGHT_BASE + 120.0f` as the max Z, which happens to equal `HEIGHT_AMP` by coincidence. If terrain height constants change, culling will silently produce artifacts.

**Fix:** Derive the range from `TerrainGenerator` constants rather than a magic literal.

### Frustum Plane Indexing Deserves a Comment

The Gribb-Hartmann plane extraction uses `vp[i][j]` which in GLM means column `i`, row `j`. This is correct but non-obvious. Worth a comment pointing to the reference.

### Water Renders as Solid

`is_opaque` returns true for water (sea level check), so water faces cull correctly, but the shader has no alpha support — water is just solid blue. This is likely intentional for the WIP but worth tracking.

### No Chunk Cache on Reload

Moving away from and back to a previously-loaded area regenerates the column from scratch. A small generation cache keyed by `ChunkColumnKey` would help in practice.

---

## Minor Nits

- **Hash distribution:** `ChunkColumnKeyHash` uses `h1 ^ (h2 * 2654435761u)`. This has poor distribution for regular integer grids where `cx == cy`. A better combine:
  ```cpp
  return h1 ^ (h2 * 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
  ```
- **`keyboard` raw pointer:** `camera_controller.hpp` takes `const bool* keyboard` (mirrors SDL's API). Consider `std::span<const bool>` for clarity.
- **FPS counter is CPU-side:** The timer accumulates before any GPU wait, so it measures CPU frame pacing, not total frame time. Fine for now, just worth knowing.

---

## Summary

For a WIP, the architecture is ahead of where the features are. The hardest problems — threading, frustum culling, AO meshing — are already solved correctly. The most impactful next steps:

1. Chunk request queue optimization (avoid full-grid rebuild per frame)
2. Water transparency (alpha blending pass)
3. Fix the hash combine for `ChunkColumnKeyHash`

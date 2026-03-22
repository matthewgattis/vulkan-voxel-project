# Vulkan Voxel Project

A Vulkan-based voxel renderer built with C++23, using SDL3 for windowing and Vulkan RAII wrappers for safe resource management.

## Prerequisites

- CMake 3.25+
- C++23 compatible compiler
- Vulkan SDK (with `glslc` shader compiler)
- Git (for vcpkg submodule)

## Getting Started

```bash
git clone --recurse-submodules <repo-url>
cd vulkan-voxel-project

# Bootstrap vcpkg
./vcpkg/bootstrap-vcpkg.sh

# Configure and build
cmake --preset default
cmake --build build

# Run tests
cd build && ctest --output-on-failure

# Run the application
./build/voxel/voxel
```

## Controls

- **Left-click drag** — Mouse look
- **WASD** — Move on XY plane
- **Space / Left Shift** — Move up / down (Z axis)
- **Tab / Left Ctrl** — Sprint (latches while moving, 2x speed)

## Project Structure

```
├── steel/          Vulkan RAII helpers (namespace: steel)
│   ├── engine      SDL3 window, Vulkan instance/device/swapchain, input, FXAA post-processing
│   ├── pipeline    Fluent graphics pipeline builder (dynamic viewport/scissor)
│   ├── buffer      Vertex/index buffer creation with staging upload
│   ├── uniform_buffer  Header-only per-frame UBO template with descriptor management
│   └── shaders/    Internal GLSL shaders (FXAA 3.11), compiled to embedded SPIR-V at build time
├── glass/          Engine abstraction layer (namespace: glass)
│   ├── shader      SPIR-V loader (stage + binary data)
│   ├── mesh        Abstract data-only mesh interface (vertices/indices spans)
│   ├── geometry    GPU-side buffers created from a Mesh
│   ├── material    Shader pipeline wrapper (with descriptor set layout for per-frame UBO)
│   ├── entity      Lightweight entity handle (index + generation)
│   ├── component_pool  Sparse-set component storage
│   ├── world       Entity manager with component operations
│   ├── view        Multi-component query iterator
│   ├── components  Transform, MeshComponent, MaterialComponent, Velocity, CameraComponent
│   ├── camera      Projection-only perspective camera (view derived from Transform)
│   ├── renderer    ECS rendering with per-frame UBO and explicit camera entity
│   └── vertex      Shared vertex format (position + normal + color)
├── voxel/          Application executable (namespace: voxel)
│   ├── application Voxel terrain renderer with spectator camera
│   ├── voxel       VoxelType enum and helpers (Grass, Dirt, Stone)
│   ├── chunk       16x16x16 voxel data storage
│   ├── chunk_mesh  Mesh generation with per-vertex AO and cross-chunk culling
│   ├── terrain     Simplex noise terrain generation
│   ├── camera_controller  Spectator camera with velocity physics and sprint
│   ├── shaders/    GLSL shaders (compiled to SPIR-V via glslc)
│   └── main        Entry point
├── docs/           Design notes and scaling documentation
└── test/           Google Test suite
```

## Dependencies

Managed via [vcpkg](https://github.com/microsoft/vcpkg) (included as a git submodule):

| Package | Purpose |
|---------|---------|
| vulkan | Graphics API |
| vulkan-memory-allocator | GPU memory management |
| glm | Linear algebra (noise, transforms) |
| sdl3 | Windowing and input |
| gtest | Testing framework |
| spdlog | Structured logging |

## CMake Presets

| Preset | Build Type | Directory |
|--------|-----------|-----------|
| `default` | Debug | `build/` |
| `release` | Release | `build-release/` |

## Architecture

Three-layer architecture: **steel** -> **glass** -> **voxel**.

**steel** wraps Vulkan initialization and rendering into RAII types (`vk::raii::*`) so resources clean up automatically. The `Engine` class provides a `begin_frame()`/`end_frame()` interface with frames-in-flight synchronization, input handling (keyboard state, mouse capture on left-click drag, delta time), and always-on FXAA 3.11 anti-aliasing (quality preset 12 with edge endpoint search). `UniformBuffer<T>` is a header-only template managing per-frame-in-flight descriptor sets and persistently mapped buffers. `PipelineBuilder` uses a fluent API with dynamic viewport/scissor. `Buffer` handles device-local vertex and index buffer creation with staging transfers.

**glass** provides engine-level abstractions. `Camera` is projection-only (fov, aspect, near, far); the view matrix is derived from the entity's `Transform` via `glm::inverse()`. The ECS (`Entity`, `ComponentPool<T>`, `World`, `View<Ts...>`) uses sparse-set storage for O(1) component operations. `Renderer` takes an explicit camera entity via `set_camera()`, computes `view_projection` each frame, and updates a per-frame UBO (set 0). Per-object model matrices are pushed via push constants. Standard components: `Transform`, `MeshComponent`, `MaterialComponent`, `Velocity`, `CameraComponent`.

**voxel** is the application. It generates terrain from simplex noise across a 4x4 chunk grid (16x16x16 voxels per chunk, Z-up). `ChunkMesh` performs per-face neighbor culling (with cross-chunk lookups via a `SolidQuery` function), per-vertex ambient occlusion (3-neighbor corner/edge detection), and AO-aware quad triangulation. Shaders use half-Lambert lighting. The spectator camera uses velocity-based physics with subtractive friction, sprint support, and frame-rate independent integration.

On macOS, MoltenVK portability extensions are automatically enabled.

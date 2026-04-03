# Vulkan Voxel Project

A work-in-progress Vulkan-based voxel renderer built with C++23 — an experiment in building a Vulkan game engine from scratch, using SDL3 for windowing and Vulkan RAII wrappers for safe resource management.

![Screenshot](docs/screenshot.png)

## Prerequisites

- CMake 3.25+
- C++23 compatible compiler
- Vulkan-capable GPU
- Git (for vcpkg submodule)

## Getting Started

```bash
git clone --recurse-submodules https://github.com/matthewgattis/vulkan-voxel-project.git
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

- **Click** — Capture mouse (for look mode)
- **Escape** — Release mouse
- **WASD** — Move on XY plane
- **Space / Left Shift** — Move up / down (Z axis)
- **Tab / Left Ctrl** — Sprint (latches while moving, 2x speed)
- **F3** — Toggle debug overlay

In VR mode, WASD movement follows the headset look direction. Mouse horizontal movement rotates the VR reference frame.

## Project Structure

```
├── steel/          Vulkan RAII helpers + OpenXR (namespace: steel)
│   ├── engine      SDL3 window, Vulkan instance/device/swapchain, deferred GPU resource destruction, FXAA post-processing, ImGui integration
│   ├── xr_system   OpenXR HMD integration (session lifecycle, stereo swapchains, head tracking, pose conversion)
│   ├── swapchain   Swapchain, depth buffer, offscreen target, scene render pass
│   ├── pipeline    Fluent graphics pipeline builder (dynamic viewport/scissor)
│   ├── buffer      Vertex/index buffer creation with staging upload
│   ├── uniform_buffer  Header-only per-frame UBO template with descriptor management
│   ├── fxaa_pass   FXAA 3.11 post-processing pass
│   ├── imgui_pass  Dear ImGui rendering pass
│   └── shaders/    Internal GLSL shaders (FXAA 3.11), compiled to embedded SPIR-V at build time
├── glass/          Engine abstraction layer (namespace: glass)
│   ├── event_dispatcher  Multi-subscriber event fan-out with RAII subscriptions and handled flag
│   ├── shader      SPIR-V loader (stage + binary data)
│   ├── mesh        Abstract data-only mesh interface (vertices/indices spans)
│   ├── geometry    GPU-side buffers created from a Mesh
│   ├── material    Shader pipeline wrapper (with descriptor set layout for per-frame UBO)
│   ├── entity      Lightweight entity handle (index + generation)
│   ├── component_pool  Sparse-set component storage
│   ├── world       Entity manager with component operations and pre-destroy callback
│   ├── view        Multi-component query iterator
│   ├── components  Transform, GeometryComponent, MaterialComponent, Velocity, CameraComponent
│   ├── camera      Projection-only perspective camera (view derived from Transform)
│   ├── renderer    ECS rendering with split view/projection UBO, explicit camera entity, automatic GPU resource cleanup
│   └── vertex      Shared vertex format (position + normal + color)
├── voxel/          Application executable (namespace: voxel)
│   ├── application Voxel terrain renderer with spectator camera and ImGui debug overlay
│   ├── voxel       VoxelType enum and helpers (Grass, Dirt, Stone, Sand, Snow, Water)
│   ├── chunk       16x16x16 voxel data storage
│   ├── chunk_mesh  Mesh generation with per-vertex AO, cross-chunk culling, and vertex welding
│   ├── chunk_manager  Multithreaded dynamic chunk loading/unloading with frustum prioritization
│   ├── terrain_generator  Multi-octave fBm terrain with TerrainColumn heightmap caching
│   ├── camera_controller  Spectator camera with velocity physics, sprint, and event-driven input
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
| imgui | Debug overlay |
| shaderc | GLSL to SPIR-V compilation (provides `glslc`) |
| gtest | Testing framework |
| openxr-loader | OpenXR HMD support |
| spdlog | Structured logging |

## CMake Presets

| Preset | Build Type | Directory |
|--------|-----------|-----------|
| `default` | Debug | `build/` |
| `release` | Release | `build-release/` |

## Architecture

Three-layer architecture: **steel** -> **glass** -> **voxel**.

### steel — Vulkan RAII engine + OpenXR

Wraps Vulkan initialization and rendering into RAII types (`vk::raii::*`).

- **Engine** provides `begin_frame()`/`end_frame()` with frames-in-flight synchronization, delta time, and a single event callback slot (typically claimed by `glass::EventDispatcher`). Split frame interface (`begin_command_buffer()` + `begin_scene_pass()`) supports XR dual-path rendering. `EngineConfig` accepts extra Vulkan extensions and a physical device query for OpenXR.
- **XrSystem** manages OpenXR HMD lifecycle via `XR_KHR_vulkan_enable`. Two-phase init: `query_requirements()` before Vulkan setup, constructor after. Owns XR session, per-eye swapchains, depth buffers, and render pass. Handles session state machine (IDLE→READY→FOCUSED→STOPPING), frame timing, pose-to-view-matrix conversion (Y-up→Z-up), and asymmetric frustum projection with Vulkan Y-flip. Graceful fallback when no HMD connected.
- **Deferred destruction** via `Engine::defer_destroy<T>()` — holds any moveable GPU resource for `MAX_FRAMES_IN_FLIGHT + 1` frames before dropping it.
- **FXAA 3.11** always-on anti-aliasing on the desktop view (quality preset 12 with edge endpoint search). Scene renders to an offscreen target; FXAA reads it and writes to the swapchain. Transparent to upper layers.
- **Swapchain** owns the swapchain, depth buffer, offscreen target, and scene render pass.
- **UniformBuffer\<T\>** — header-only template managing per-frame-in-flight descriptor sets and persistently mapped buffers.
- **PipelineBuilder** — fluent API with dynamic viewport/scissor.
- **Buffer** — device-local vertex/index buffer creation with staging transfers.

### glass — engine abstractions

Provides ECS, rendering, and event dispatch on top of steel.

- **EventDispatcher** registers as Engine's sole event callback and fans out SDL events to multiple subscribers. Callbacks receive `bool& handled` for event consumption. `subscribe()` returns an RAII `Subscription` that unsubscribes on destruction.
- **ECS** (`Entity`, `ComponentPool<T>`, `World`, `View<Ts...>`) uses sparse-set storage for O(1) component operations. `World` supports a pre-destroy callback for automatic GPU resource cleanup.
- **Camera** is projection-only (fov, aspect, near, far); view matrix derived from Transform via `glm::inverse()`.
- **Renderer** updates a per-frame UBO (set 0) with split view/projection matrices. `bind_world()` registers a destroy callback so `GeometryComponent`-owned GPU buffers are automatically deferred-destroyed. Per-object model matrices use push constants. XR mode uses per-eye UBOs and dedicated `render_xr_eyes()`/`render_desktop_companion()` methods.
- **Components**: Transform, GeometryComponent (owns `unique_ptr<Geometry>`), MaterialComponent, Velocity, CameraComponent.

### voxel — application

- **ChunkManager** dynamically loads/unloads terrain around the camera. In-frustum chunks are prioritized, with distance as tiebreaker. Generation runs on a pool of `std::jthread` workers with cooperative cancellation.
- **TerrainGenerator** produces terrain from 6-octave fBm simplex noise with a power curve for sharper peaks (16x16x16 voxels per chunk, Z-up). `TerrainColumn` precomputes the heightmap and gradient per column.
- **Biomes**: grass, dirt, stone, slope-aware sand near shorelines, snow above a noise-varying snow line, and opaque water below sea level.
- **ChunkMesh** performs per-face neighbor culling (cross-chunk), per-vertex ambient occlusion, AO-aware quad triangulation, and vertex welding via FNV-1a hashing.
- **Shaders** compute half-Lambert lighting and spherical exponential-squared distance fog in the vertex shader.
- **CameraController** subscribes to EventDispatcher for keyboard and mouse events. Velocity-based physics with subtractive friction, sprint support, and frame-rate independent integration. In VR, movement direction follows headset look direction instead of mouse yaw.
- **Application** subscribes to EventDispatcher for ImGui forwarding, mouse capture (click to capture, Escape to release), and key shortcuts (F3 debug overlay toggle). Dual-path main loop: XR stereo rendering + desktop companion mirror when HMD connected, desktop-only otherwise.

On macOS, MoltenVK portability extensions are automatically enabled.

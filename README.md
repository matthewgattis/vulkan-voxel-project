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

## Project Structure

```
├── steel/          Vulkan RAII helpers (namespace: steel)
│   ├── engine      SDL3 window, Vulkan instance/device/swapchain, depth buffer, render pass, FXAA post-processing
│   ├── pipeline    Fluent graphics pipeline builder (dynamic viewport/scissor)
│   ├── buffer      Vertex/index buffer creation with staging upload
│   └── shaders/    Internal GLSL shaders (FXAA), compiled to embedded SPIR-V at build time
├── glass/          Engine abstraction layer (namespace: glass)
│   ├── shader      SPIR-V loader (stage + binary data)
│   ├── mesh        Abstract data-only mesh interface (vertices/indices spans)
│   ├── geometry    GPU-side buffers created from a Mesh
│   ├── material    Shader pipeline wrapper
│   ├── renderable  Bundles Geometry + Material into a drawable unit
│   ├── scene_node  Scene graph with transforms
│   ├── camera      Perspective camera with view/projection matrices
│   ├── renderer    Scene graph traversal and frame rendering
│   └── vertex      Shared vertex format (position + normal + color)
├── voxel/          Application executable (namespace: voxel)
│   ├── application Colored cube renderer using glass API
│   ├── shaders/    GLSL shaders (compiled to SPIR-V via glslc)
│   └── main        Entry point
└── test/           Google Test suite (90 tests)
```

## Dependencies

Managed via [vcpkg](https://github.com/microsoft/vcpkg) (included as a git submodule):

| Package | Purpose |
|---------|---------|
| vulkan | Graphics API |
| vulkan-memory-allocator | GPU memory management |
| glm | Linear algebra |
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

**steel** wraps Vulkan initialization and rendering into RAII types (`vk::raii::*`) so resources clean up automatically. The `Engine` class provides a `begin_frame()`/`end_frame()` interface with frames-in-flight synchronization (`MAX_FRAMES_IN_FLIGHT` = 2), plus `wait_idle()` for clean shutdown. Command buffers, fences, and acquire semaphores are per frame in flight; present semaphores are per swapchain image. Engine includes always-on FXAA anti-aliasing: the scene renders to an offscreen target, then an FXAA fullscreen pass reads it via a combined image sampler and writes to the swapchain. The FXAA shaders (`fullscreen.vert`, `fxaa.frag`) live in `steel/shaders/`, are compiled to SPIR-V at build time, and embedded as `constexpr` arrays in a generated header (not checked into git). The FXAA pipeline is built directly, separate from `PipelineBuilder`. The external `begin_frame()`/`end_frame()` API is unchanged — glass and voxel are unaware of FXAA. Engine auto-selects the GPU based on capability checks (queue families, swapchain extension, surface formats, depth format), and the depth format is selected at runtime. Supports window resize via swapchain recreation, high-DPI rendering, and auto-selects the largest fitting 4:3 resolution for the display. `PipelineBuilder` takes SPIR-V bytecode at construction and uses a fluent API for remaining pipeline state; viewport and scissor are dynamic state set at draw time, so pipelines do not need to be recreated on window resize. `Buffer` handles device-local vertex and index buffer creation with staging transfers. Diagnostic logging uses spdlog.

**glass** sits between steel and the application, providing engine-level abstractions. `Shader` loads SPIR-V from disk and exposes its bytecode and stage. `Mesh` is a data-only abstract interface returning `vertices()` and `indices()` spans. `Geometry` is created from a `Mesh`, uploading vertex and index data to the GPU. `Material` takes `Shader` objects and builds a pipeline, and exposes its pipeline layout for push constant binding. `Renderable` bundles a `Geometry` and `Material` into a single drawable unit. `SceneNode` provides a simple scene graph with transforms, referencing a `Renderable`. `Camera` provides perspective projection with Vulkan Y-flip correction. `Renderer` takes a `Camera&` (non-const) and traverses the scene graph, pushing MVP matrices via push constants per draw call. The renderer updates the camera's aspect ratio each frame from the engine's current extent so resize works correctly. Application code uses glass and never touches Vulkan objects directly.

**voxel** is the application. It currently renders a colored cube with a perspective camera positioned at (2,2,2) looking at the origin, by implementing `glass::Mesh`, creating `Shader`, `Geometry`, `Material`, `Camera`, and `Renderable` objects, then building a scene graph for the `Renderer`. Shaders are written in GLSL 450 and compiled to SPIR-V at build time.

On macOS, MoltenVK portability extensions are automatically enabled.

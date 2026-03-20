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
│   ├── engine      SDL3 window, Vulkan instance/device/swapchain, depth buffer, render pass
│   ├── pipeline    Fluent graphics pipeline builder
│   └── buffer      Vertex/index buffer creation with staging upload
├── glass/          Engine abstraction layer (namespace: glass)
│   ├── shader      SPIR-V loader (stage + binary data)
│   ├── mesh        Abstract data-only mesh interface (vertices/indices spans)
│   ├── geometry    GPU-side buffers created from a Mesh
│   ├── material    Shader pipeline wrapper
│   ├── renderable  Bundles Geometry + Material into a drawable unit
│   ├── scene_node  Scene graph with transforms
│   ├── renderer    Scene graph traversal and frame rendering
│   └── vertex      Shared vertex format (position + normal + color)
├── voxel/          Application executable (namespace: voxel)
│   ├── application Triangle renderer using glass API
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

**steel** wraps Vulkan initialization and rendering into RAII types (`vk::raii::*`) so resources clean up automatically. The `Engine` class provides a `begin_frame()`/`end_frame()` interface with per-swapchain-image synchronization, plus `wait_idle()` for clean shutdown. `PipelineBuilder` takes SPIR-V bytecode at construction and uses a fluent API for remaining pipeline state. `Buffer` handles device-local vertex and index buffer creation with staging transfers. Diagnostic logging uses spdlog.

**glass** sits between steel and the application, providing engine-level abstractions. `Shader` loads SPIR-V from disk and exposes its bytecode and stage. `Mesh` is a data-only abstract interface returning `vertices()` and `indices()` spans. `Geometry` is created from a `Mesh`, uploading vertex and index data to the GPU. `Material` takes `Shader` objects and builds a pipeline. `Renderable` bundles a `Geometry` and `Material` into a single drawable unit. `SceneNode` provides a simple scene graph with transforms, referencing a `Renderable`. `Renderer` traverses the scene graph and drives the frame loop. Application code uses glass and never touches Vulkan objects directly.

**voxel** is the application. It currently renders a colored triangle by implementing `glass::Mesh`, creating `Shader`, `Geometry`, `Material`, and `Renderable` objects, then building a scene graph for the `Renderer`. Shaders are written in GLSL 450 and compiled to SPIR-V at build time.

On macOS, MoltenVK portability extensions are automatically enabled.

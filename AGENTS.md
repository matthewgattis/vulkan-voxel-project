# AGENTS.md

## Project Overview

Vulkan voxel renderer using C++23 with CMake and vcpkg. Three-layer architecture: `steel` (Vulkan RAII wrappers) -> `glass` (engine abstractions) -> `voxel` (application).

## Build

```bash
cmake --preset default
cmake --build build
cd build && ctest --output-on-failure
```

Requires: CMake 3.25+, C++23 compiler, Vulkan SDK with `glslc`, spdlog (via vcpkg).

## Code Organization

- **Top-level CMakeLists.txt**: Only finds packages and adds subdirectories. Do not add targets here.
- **steel/**: Vulkan RAII engine library. Namespace `steel`. Links against Vulkan, GLM, SDL3.
- **glass/**: Engine abstraction layer. Namespace `glass`. Links against `steel`. Provides meshes, materials, scene graph, and rendering abstractions built on top of steel's Vulkan wrappers.
- **voxel/**: Application executable. Namespace `voxel`. Links against `steel` and `glass`.
- **test/**: Google Test suite. Links against `steel`, `glass`, and GTest.

Each subdirectory has its own `CMakeLists.txt`.

## Conventions

- **C++ standard**: C++23, no extensions
- **Namespaces**: `steel` for Vulkan RAII wrappers, `glass` for engine abstractions, `voxel` for application code
- **Vulkan**: Use `vk::raii::` types exclusively (RAII wrappers, no manual cleanup)
- **Headers**: `<module>/include/<module>/` layout (e.g., `steel/include/steel/engine.hpp`)
- **Shaders**: GLSL 450 in `voxel/shaders/`, compiled to SPIR-V by `glslc` at build time
- **Tests**: No GPU required. Test struct layouts, type traits, Vulkan struct construction, and utilities.
- **Vulkan HPP structs**: Use member assignment or constructor syntax, not C++20 designated initializers (they do not work reliably with Vulkan HPP types)

## Key Interfaces

### steel::Engine
- `Engine(title, width, height)` — creates window and initializes Vulkan
- `begin_frame()` → `const vk::raii::CommandBuffer*` (nullptr if frame unavailable)
- `end_frame()` — submits and presents
- `wait_idle()` — waits for device idle (used for clean shutdown)
- `poll_events()` → `bool` (false = quit requested)
- Per-swapchain-image synchronization via `MAX_FRAMES_IN_FLIGHT` (defined in engine.hpp)
- Accessors: `device()`, `physical_device()`, `render_pass()`, `extent()`, `command_pool()`, `graphics_queue()`, `graphics_family()`, `color_format()`, `depth_format()`
- Uses spdlog for diagnostic logging

### steel::PipelineBuilder
- Constructor: `PipelineBuilder(device, vert_spirv, frag_spirv)` — takes SPIR-V bytecode upfront
- Fluent API for remaining state: `set_vertex_input(bindings, attrs)`, `set_topology()`, `set_polygon_mode()`, `set_cull_mode()`, `set_depth_test()`
- `build(render_pass, layout, extent)` → `vk::raii::Pipeline`

### steel::Buffer
- `Buffer::create_vertex_buffer(device, physical_device, command_pool, queue, data)` — staging upload to device-local vertex buffer
- `Buffer::create_index_buffer(device, physical_device, command_pool, queue, data)` — staging upload to device-local index buffer
- `Buffer::create(device, physical_device, size, usage, memory_properties)` — general buffer creation
- `handle()` → `vk::Buffer`, `size()` → `vk::DeviceSize`

### glass::Shader
- `Shader::load(stage, spirv_path)` — loads SPIR-V binary from file
- `spirv()` → `span<const uint32_t>`, `stage()` → `vk::ShaderStageFlagBits`
- Move-only

### glass::Vertex
- `position` (vec3) + `normal` (vec3) + `color` (vec3) = 36 bytes, standard layout
- `binding_description()` — vertex input binding at binding 0
- `attribute_descriptions()` — three R32G32B32Sfloat attributes (position at 0, normal at 1, color at 2)

### glass::Mesh
- Data-only abstract interface for mesh data
- `vertices()` → `span<const Vertex>`, `indices()` → `span<const uint32_t>`

### glass::Geometry
- Created from a `Mesh`: `Geometry::create(engine, mesh)` — uploads vertex and index data via staging buffers
- `bind(cmd)`, `draw(cmd)` — binds vertex/index buffers and issues draw call
- Move-only

### glass::Material
- Owns a `vk::raii::Pipeline` and `vk::raii::PipelineLayout`
- Move-only (not copyable, since it owns Vulkan RAII handles)
- `Material::create(engine, vertex_shader, fragment_shader)` — takes `Shader` objects (not file paths)
- `bind(cmd)` — binds the pipeline

### glass::Renderable
- Bundles a `Geometry` and `Material` into a single drawable unit
- `Renderable(geometry, material)` — move-constructs from both
- `geometry()`, `material()` — const accessors
- Move-only

### glass::Renderer
- Traverses scene graph and renders each frame
- Uses `Engine::begin_frame()`/`end_frame()` to drive the render loop

### glass::SceneNode
- Simple scene graph node: `transform` (mat4), `renderable` (`const Renderable*`), `children`
- Default-constructed with identity transform, null renderable, empty children

## Adding New Code

- New steel features: add files under `steel/src/` and `steel/include/steel/`, update `steel/CMakeLists.txt`
- New glass features: add files under `glass/src/` and `glass/include/glass/`, update `glass/CMakeLists.txt`
- New application features: add under `voxel/src/` and `voxel/include/voxel/`, update `voxel/CMakeLists.txt`
- New tests: add `.cpp` files under `test/`, update `test/CMakeLists.txt`
- New shaders: add `.vert`/`.frag` under `voxel/shaders/`, add to `SHADERS` list in `voxel/CMakeLists.txt`
- New dependencies: add to `vcpkg.json`, `find_package()` in top-level CMakeLists.txt, link in the appropriate subdirectory

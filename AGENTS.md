# AGENTS.md

## Project Overview

Vulkan voxel renderer using C++23 with CMake and vcpkg. Three-layer architecture: `steel` (Vulkan RAII wrappers) -> `glass` (engine abstractions with ECS) -> `voxel` (application). The application renders procedurally generated voxel terrain using simplex noise, with multithreaded chunk loading, per-vertex ambient occlusion, vertex welding, half-Lambert lighting, and a spectator camera with velocity-based physics. Always-on FXAA 3.11 anti-aliasing and Dear ImGui debug overlay are applied as post-processing passes inside `steel::Engine`.

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
- **glass/**: Engine abstraction layer. Namespace `glass`. Links against `steel`. Provides meshes, materials, ECS (Entity Component System), and rendering abstractions built on top of steel's Vulkan wrappers.
- **voxel/**: Application executable. Namespace `voxel`. Links against `glass`.
- **test/**: Google Test suite. Links against `steel`, `glass`, and GTest.

Each subdirectory has its own `CMakeLists.txt`.

## Conventions

- **C++ standard**: C++23, no extensions
- **Namespaces**: `steel` for Vulkan RAII wrappers, `glass` for engine abstractions, `voxel` for application code
- **Vulkan**: Use `vk::raii::` types exclusively (RAII wrappers, no manual cleanup)
- **Headers**: `<module>/include/<module>/` layout (e.g., `steel/include/steel/engine.hpp`)
- **Shaders**: GLSL 450 in `voxel/shaders/` and `steel/shaders/`, compiled to SPIR-V by `glslc` at build time. Application shaders use a per-frame UBO (set 0, binding 0) for `view_projection` and push constants (`mat4 model`) for per-object transforms. Steel's internal FXAA shaders (`fullscreen.vert`, `fxaa.frag`) are compiled to SPIR-V and embedded as `constexpr` arrays in a generated header (not checked into git).
- **Front face**: Default front face is clockwise (`vk::FrontFace::eClockwise`)
- **Coordinate system**: Z is up, terrain extends across the XY plane
- **Push constants**: Used for per-object model transforms, pushed per draw call
- **Descriptor sets**: Set 0 = per-frame UBO (view_projection), set 1 = reserved for per-material (future)
- **Tests**: No GPU required. Test struct layouts, type traits, Vulkan struct construction, and utilities.
- **Vulkan HPP structs**: Use member assignment or constructor syntax, not C++20 designated initializers (they do not work reliably with Vulkan HPP types)

## Key Interfaces

### steel::Engine
- `Engine(title)` — creates window and initializes Vulkan. Auto-selects largest fitting 4:3 resolution from predefined list for the primary display.
- High-DPI support via `SDL_WINDOW_HIGH_PIXEL_DENSITY`
- `begin_frame()` -> `const vk::raii::CommandBuffer*` (nullptr if frame unavailable). Sets dynamic viewport and scissor from the current extent. Flushes deferred destruction queue.
- `end_frame()` — submits and presents
- FXAA 3.11 post-processing: the scene renders to an offscreen target, then an FXAA fullscreen pass (quality preset 12 with edge endpoint search) reads it via a combined image sampler descriptor and writes to the swapchain. The FXAA pipeline is built directly, separate from `PipelineBuilder`. The `begin_frame()`/`end_frame()` API is unchanged — glass and voxel are unaware of FXAA.
- `wait_idle()` — waits for device idle (used for clean shutdown)
- `poll_events()` -> `bool` (false = quit requested). Handles quit, resize, and delta time. Forwards all other events via optional event callback.
- `set_event_callback(fn)` — optional per-event callback for application-level input handling
- `keyboard_state()` -> `const bool*` — SDL keyboard state array
- `delta_time()` — frame delta in seconds, clamped to 0.1s max
- `current_frame()` — current frame-in-flight index
- `defer_destroy<T>(resource)` — type-erased deferred destruction, holds resource for `MAX_FRAMES_IN_FLIGHT + 1` frames
- `window()` -> `SDL_Window*`
- ImGui: `imgui_begin()`, `imgui_end()`, `imgui_enabled()`, `set_imgui_enabled()`, `imgui_process_event()`
- Frames in flight: `MAX_FRAMES_IN_FLIGHT` (2, defined in engine.hpp)
- Accessors: `device()`, `physical_device()`, `render_pass()`, `extent()`, `command_pool()`, `graphics_queue()`, `graphics_family()`, `color_format()`, `depth_format()`, `allocator()`

### steel::UniformBuffer\<T\>
- Header-only template encapsulating descriptor set layout, pool, per-frame-in-flight sets, buffers, and persistent mapping
- `create(engine, stages)` — creates layout, pool, sets, buffers with persistent mapping
- `update(frame_index, data)` — memcpy to mapped buffer
- `bind(cmd, layout, set_index, frame_index)` — binds descriptor set
- `layout()` -> `const vk::raii::DescriptorSetLayout&`

### steel::PipelineBuilder
- Constructor: `PipelineBuilder(device, vert_spirv, frag_spirv)` — takes SPIR-V bytecode upfront
- Fluent API for remaining state: `set_vertex_input(bindings, attrs)`, `set_topology()`, `set_polygon_mode()`, `set_cull_mode()`, `set_depth_test()`
- Default front face is `eClockwise` (matching Vulkan convention with Y-flipped projection)
- `build(render_pass, layout)` -> `vk::raii::Pipeline` — viewport and scissor are dynamic state

### steel::Buffer
- `Buffer::create_vertex_buffer(...)` — staging upload to device-local vertex buffer
- `Buffer::create_index_buffer(...)` — staging upload to device-local index buffer
- `Buffer::create(...)` — general buffer creation
- `map()`, `unmap()` — host-visible memory access

### glass::Camera
- Projection-only: `Camera(fov_degrees, aspect_ratio, near_plane, far_plane)`
- `set_aspect_ratio(float)` — updated by renderer each frame
- `projection()` -> `const glm::mat4&` — Y-flipped for Vulkan
- View matrix is derived from `glm::inverse(Transform.matrix)` in the renderer

### glass::Renderer
- `Renderer(engine)` — creates per-frame UBO for `view_projection`
- `bind_world(world)` — registers pre-destroy callback for automatic GPU resource cleanup (defers `GeometryComponent`-owned geometry via Engine)
- `set_camera(entity)` — sets the active camera entity
- `render_frame(world)` — computes view_projection from camera entity's Transform and CameraComponent, updates UBO, renders all entities with Transform + GeometryComponent + MaterialComponent
- `frame_descriptor_layout()` — exposes UBO descriptor set layout for material pipeline creation

### glass::Components
- `Transform` — `glm::mat4 matrix` (default identity)
- `GeometryComponent` — `std::unique_ptr<Geometry>` (owns GPU buffers, automatically deferred-destroyed on entity destruction)
- `MaterialComponent` — `const Material*` (non-owning, long-lived)
- `Velocity` — `glm::vec3 linear` (default zero)
- `CameraComponent` — `Camera camera`

### glass::Entity, World, View
- `Entity` — lightweight handle: `uint32_t index` + `uint32_t generation`
- `World` — entity manager with create/destroy, component operations (`add`, `remove`, `get`, `has`), `view<Ts...>()` for multi-component queries. Supports `set_on_destroy(callback)` — called before components are removed, enabling GPU resource capture for deferred destruction.
- `View<Ts...>` — iterates smallest pool, filters by all requested types, `each(fn)` callback

### glass::Material
- `Material::create(engine, vertex_shader, fragment_shader, frame_descriptor_layout)` — pipeline layout includes descriptor set layout at set 0 and push constant range for model matrix
- `bind(cmd)`, `layout()` — pipeline binding and layout access

## Voxel Application

### voxel::Application
- Owns Engine, Renderer, World, Material, ChunkManager, TerrainGenerator, CameraController
- Sets up Engine event callback for mouse capture, ImGui toggle (F3), and mouse motion accumulation
- Calls `renderer_.bind_world(world_)` for automatic GPU resource cleanup
- ImGui debug overlay shows FPS and VMA memory statistics

### voxel::ChunkManager
- Dynamic chunk loading/unloading around camera within square radius 8 (17x17 grid = 289 columns)
- Each column spans multiple vertical slices (chunks stacked in Z)
- Multithreaded: pool of `std::jthread` workers (up to 4) pull from a distance-sorted `std::priority_queue` (closest chunks first)
- Workers generate voxels + mesh on CPU, push results to main thread via result queue
- Main thread consumes results: creates `Geometry` (GPU upload) and ECS entities with `GeometryComponent`
- Workers check atomic camera position to skip stale out-of-range requests
- Unloading: `world_.destroy(entity)` — GPU resource cleanup is automatic via World's pre-destroy callback

### voxel::Chunk
- Pure voxel data: 16x16x16 flat array indexed as `x + y*16 + z*256`
- `get(x, y, z)`, `set(x, y, z, type)`, `in_bounds()`, chunk coordinates `cx()`, `cy()`, `cz()`

### voxel::ChunkMesh
- Implements `glass::Mesh`. Constructor takes a `Chunk` and a `SolidQuery` function
- Generates vertices/indices with per-face neighbor culling, per-vertex AO, AO-aware quad triangulation, and vertex welding
- Vertex welding: FNV-1a hash over raw 36-byte Vertex struct, `std::unordered_map` deduplication. Vertices with identical position, normal, and AO-modulated color are welded.
- `SolidQuery = std::function<bool(int wx, int wy, int wz)>` — enables cross-chunk neighbor lookups
- Per-vertex AO: checks 3 adjacent voxels per vertex (2 sides + 1 corner). If both sides solid, AO = 0 (darkest). Brightness mapped as {0.25, 0.5, 0.75, 1.0}.
- Quad flip: triangulation diagonal chosen based on AO values to avoid visual artifacts

### voxel::TerrainGenerator
- Generates terrain from simplex noise: `terrain_height(wx, wy)` using `glm::simplex`
- `is_solid_at(wx, wy, wz)` — deterministic world-space solid query from noise (used for cross-chunk culling and AO)
- `fill_chunk(chunk)` — fills a chunk with voxel data based on noise
- `column_height()` — number of vertical chunks per column
- Fill layers: Grass (top), Dirt (1-3 below surface), Stone (deeper)

### voxel::CameraController
- Spectator camera: WASD movement on XY plane, Space/Shift for Z, left-click-drag for mouse look
- `update(dt, mouse_dx, mouse_dy, keyboard, world, camera_entity)` — decoupled from Engine
- `position()` — current camera position (used by ChunkManager for loading radius)
- Velocity-based physics: acceleration + subtractive friction (proportional to speed), frame-rate independent
- Sprint: Tab or Ctrl latches sprint on while moving, releases when all move keys released
- Base speed 11 units/s, sprint speed 22 units/s
- Camera convention: OpenGL look-along -Z with base rotation for Z-up world

## Adding New Code

- New steel features: add files under `steel/src/` and `steel/include/steel/`, update `steel/CMakeLists.txt`
- New glass features: add files under `glass/src/` and `glass/include/glass/`, update `glass/CMakeLists.txt`
- New application features: add under `voxel/src/` and `voxel/include/voxel/`, update `voxel/CMakeLists.txt`
- New tests: add `.cpp` files under `test/`, update `test/CMakeLists.txt`
- New application shaders: add `.vert`/`.frag` under `voxel/shaders/`, add to `SHADERS` list in `voxel/CMakeLists.txt`
- New engine shaders: add `.vert`/`.frag` under `steel/shaders/`, update `steel/CMakeLists.txt` to compile and embed them
- New dependencies: add to `vcpkg.json`, `find_package()` in top-level CMakeLists.txt, link in the appropriate subdirectory

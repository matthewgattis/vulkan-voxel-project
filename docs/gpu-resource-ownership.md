# GPU Resource Ownership & Deferred Destruction

## Current Design

GPU resources referenced by in-flight frames cannot be destroyed immediately. `Engine::defer_destroy<T>()` holds any moveable resource for `MAX_FRAMES_IN_FLIGHT + 1` frames before dropping it.

### Geometry (implemented)

- `GeometryComponent` owns `std::unique_ptr<Geometry>` — glass owns the GPU resource
- `World` fires an `on_destroy` callback before removing components
- `Renderer::bind_world()` registers the callback: when an entity with a `GeometryComponent` is destroyed, the geometry is moved into Engine's deferred destruction queue
- Application creates a Geometry, moves it into GeometryComponent, and never manages its lifetime

### Material (current: no deferred destruction needed)

- `MaterialComponent` holds a raw `const Material*` (non-owning)
- Materials are long-lived and shared across many entities (e.g., one material for all chunks)
- Created at startup, destroyed at shutdown after `wait_idle()` — no in-flight conflict

## Future: Shared GPU Resources

If we need dynamic materials (created/destroyed at runtime) or shared geometry (same mesh on multiple entities), the current `unique_ptr` model won't work. Considerations:

- **Shared geometry**: Multiple entities referencing the same `Geometry` (e.g., instancing). Requires `shared_ptr<Geometry>` on the component.
- **Dynamic materials**: Materials created/destroyed while frames are in flight. Requires the same deferred destruction treatment as geometry, plus shared ownership since many entities reference one material.
- **Problem with `shared_ptr`**: When the last reference drops, destruction happens immediately — bypassing deferred destruction. The deferred path only works if the last drop goes through the `on_destroy` callback.
- **Possible solution**: Custom deleter on `shared_ptr` that routes through `Engine::defer_destroy` instead of immediate `delete`. This requires the `shared_ptr` to capture an Engine reference at creation time, e.g., via a factory like `Engine::make_deferred<Geometry>(...)`.
- **Alternative**: A resource registry in glass that tracks all live GPU resources by handle, with ref-counted handles instead of raw `shared_ptr`.

No action needed until we have a concrete use case for shared geometry or dynamic materials.

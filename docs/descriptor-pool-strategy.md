# Descriptor Pool Strategy

## Current Approach

Each `steel::UniformBuffer<T>` creates its own `vk::DescriptorPool` sized to exactly `MAX_FRAMES_IN_FLIGHT` (2) sets. This is simple and self-contained — each UBO manages its own lifecycle with no external dependencies.

## Scaling Concern

As the system grows (e.g., 50+ materials each with their own `UniformBuffer<MaterialUniforms>`), this means 50+ small descriptor pools. Vulkan drivers handle many small pools fine, but it's unnecessary overhead compared to a shared pool.

## Future Options

1. **Engine-owned shared pool** — Engine creates a single pool sized generously (e.g., 100+ uniform buffer descriptors). `UniformBuffer::create` takes a pool reference instead of creating its own. Simple, but requires guessing or configuring the max count.

2. **DescriptorAllocator in steel** — A wrapper that manages multiple pools, creating new ones on demand when the current pool fills up. This is a common pattern in production Vulkan engines (e.g., vkguide.dev's descriptor allocator). More robust, no upfront size guessing.

3. **Keep per-UBO pools** — Valid until the pool count becomes a measurable problem. Revisit when per-material uniforms land and material counts grow.

## When to Revisit

When per-material uniforms are implemented and material counts exceed a handful. At that point, option 2 (DescriptorAllocator) is likely the right move.

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <cstddef>
#include <span>

namespace steel {

class Buffer {
public:
    // Create a device-local vertex buffer and upload data via a staging buffer.
    static Buffer create_vertex_buffer(
        const vk::raii::Device&         device,
        const vk::raii::PhysicalDevice& physical_device,
        const vk::raii::CommandPool&    command_pool,
        const vk::raii::Queue&          queue,
        std::span<const std::byte>      data);

    // Create a device-local index buffer and upload data via a staging buffer.
    static Buffer create_index_buffer(
        const vk::raii::Device&         device,
        const vk::raii::PhysicalDevice& physical_device,
        const vk::raii::CommandPool&    command_pool,
        const vk::raii::Queue&          queue,
        std::span<const std::byte>      data);

    // Create a buffer with specified usage and memory properties (no staging).
    static Buffer create(
        const vk::raii::Device&         device,
        const vk::raii::PhysicalDevice& physical_device,
        vk::DeviceSize                  size,
        vk::BufferUsageFlags            usage,
        vk::MemoryPropertyFlags         memory_properties);

    vk::Buffer     handle() const { return *buffer_; }
    vk::DeviceSize size()   const { return size_; }

    Buffer(Buffer&&) = default;
    Buffer& operator=(Buffer&&) = default;

private:
    Buffer() = default;

    static uint32_t find_memory_type(
        const vk::raii::PhysicalDevice& physical_device,
        uint32_t                        type_filter,
        vk::MemoryPropertyFlags         properties);

    vk::raii::Buffer       buffer_ {nullptr};
    vk::raii::DeviceMemory memory_ {nullptr};
    vk::DeviceSize         size_   = 0;
};

} // namespace steel

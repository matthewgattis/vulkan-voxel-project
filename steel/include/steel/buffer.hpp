#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>
#include <cstddef>
#include <span>

namespace steel {

class Buffer {
public:
    // Create a device-local vertex buffer and upload data via a staging buffer.
    static Buffer create_vertex_buffer(
        const vk::raii::Device&      device,
        VmaAllocator                 allocator,
        const vk::raii::CommandPool& command_pool,
        const vk::raii::Queue&       queue,
        std::span<const std::byte>   data);

    // Create a device-local index buffer and upload data via a staging buffer.
    static Buffer create_index_buffer(
        const vk::raii::Device&      device,
        VmaAllocator                 allocator,
        const vk::raii::CommandPool& command_pool,
        const vk::raii::Queue&       queue,
        std::span<const std::byte>   data);

    // Create a buffer with specified usage and memory properties (no staging).
    static Buffer create(
        VmaAllocator            allocator,
        vk::DeviceSize          size,
        vk::BufferUsageFlags    usage,
        VmaMemoryUsage          memory_usage,
        VmaAllocationCreateFlags flags = 0);

    vk::Buffer     handle() const { return vk::Buffer{buffer_}; }
    vk::DeviceSize size()   const { return size_; }

    void* map();
    void  unmap();
    void* mapped_data() const;

    ~Buffer();
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

private:
    Buffer() = default;

    VmaAllocator    allocator_  = VK_NULL_HANDLE;
    VkBuffer        buffer_     = VK_NULL_HANDLE;
    VmaAllocation   allocation_ = VK_NULL_HANDLE;
    vk::DeviceSize  size_       = 0;
};

} // namespace steel

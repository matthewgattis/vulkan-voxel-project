#include <steel/buffer.hpp>

#include <cstring>
#include <stdexcept>

namespace steel {

Buffer Buffer::create_vertex_buffer(
    const vk::raii::Device&         device,
    const vk::raii::PhysicalDevice& physical_device,
    const vk::raii::CommandPool&    command_pool,
    const vk::raii::Queue&          queue,
    std::span<const std::byte>      data) {

    vk::DeviceSize size = data.size();

    // Create staging buffer (host-visible)
    Buffer staging = create(
        device, physical_device, size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // Map and copy data into staging buffer
    void* mapped = staging.memory_.mapMemory(0, size);
    std::memcpy(mapped, data.data(), size);
    staging.memory_.unmapMemory();

    // Create device-local vertex buffer
    Buffer vertex_buf = create(
        device, physical_device, size,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    // Copy via one-shot command buffer
    vk::CommandBufferAllocateInfo alloc_info{
        *command_pool,
        vk::CommandBufferLevel::ePrimary,
        1,
    };
    auto cmd_buffers = device.allocateCommandBuffers(alloc_info);
    auto& cmd = cmd_buffers[0];

    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd.copyBuffer(*staging.buffer_, *vertex_buf.buffer_, vk::BufferCopy{0, 0, size});
    cmd.end();

    vk::SubmitInfo submit_info{{}, {}, *cmd};
    queue.submit(submit_info);
    queue.waitIdle();

    return vertex_buf;
}

Buffer Buffer::create_index_buffer(
    const vk::raii::Device&         device,
    const vk::raii::PhysicalDevice& physical_device,
    const vk::raii::CommandPool&    command_pool,
    const vk::raii::Queue&          queue,
    std::span<const std::byte>      data) {

    vk::DeviceSize size = data.size();

    // Create staging buffer (host-visible)
    Buffer staging = create(
        device, physical_device, size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // Map and copy data into staging buffer
    void* mapped = staging.memory_.mapMemory(0, size);
    std::memcpy(mapped, data.data(), size);
    staging.memory_.unmapMemory();

    // Create device-local index buffer
    Buffer index_buf = create(
        device, physical_device, size,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    // Copy via one-shot command buffer
    vk::CommandBufferAllocateInfo alloc_info{
        *command_pool,
        vk::CommandBufferLevel::ePrimary,
        1,
    };
    auto cmd_buffers = device.allocateCommandBuffers(alloc_info);
    auto& cmd = cmd_buffers[0];

    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd.copyBuffer(*staging.buffer_, *index_buf.buffer_, vk::BufferCopy{0, 0, size});
    cmd.end();

    vk::SubmitInfo submit_info{{}, {}, *cmd};
    queue.submit(submit_info);
    queue.waitIdle();

    return index_buf;
}

Buffer Buffer::create(
    const vk::raii::Device&         device,
    const vk::raii::PhysicalDevice& physical_device,
    vk::DeviceSize                  size,
    vk::BufferUsageFlags            usage,
    vk::MemoryPropertyFlags         memory_properties) {

    Buffer buf;
    buf.size_ = size;

    vk::BufferCreateInfo buffer_info{
        {},
        size,
        usage,
        vk::SharingMode::eExclusive,
    };

    buf.buffer_ = vk::raii::Buffer{device, buffer_info};

    auto mem_requirements = buf.buffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{
        mem_requirements.size,
        find_memory_type(physical_device, mem_requirements.memoryTypeBits, memory_properties),
    };

    buf.memory_ = vk::raii::DeviceMemory{device, alloc_info};
    buf.buffer_.bindMemory(*buf.memory_, 0);

    return buf;
}

uint32_t Buffer::find_memory_type(
    const vk::raii::PhysicalDevice& physical_device,
    uint32_t                        type_filter,
    vk::MemoryPropertyFlags         properties) {

    auto mem_props = physical_device.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

} // namespace steel

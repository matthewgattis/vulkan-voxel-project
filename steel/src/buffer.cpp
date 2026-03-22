#include <steel/buffer.hpp>

#include <cstring>
#include <stdexcept>

namespace steel {

Buffer::~Buffer() {
    if (buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : allocator_{other.allocator_}
    , buffer_{other.buffer_}
    , allocation_{other.allocation_}
    , size_{other.size_}
{
    other.buffer_ = VK_NULL_HANDLE;
    other.allocation_ = VK_NULL_HANDLE;
    other.size_ = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
        }
        allocator_ = other.allocator_;
        buffer_ = other.buffer_;
        allocation_ = other.allocation_;
        size_ = other.size_;
        other.buffer_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
        other.size_ = 0;
    }
    return *this;
}

Buffer Buffer::create(
    VmaAllocator             allocator,
    vk::DeviceSize           size,
    vk::BufferUsageFlags     usage,
    VmaMemoryUsage           memory_usage,
    VmaAllocationCreateFlags flags) {

    Buffer buf;
    buf.allocator_ = allocator;
    buf.size_ = size;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = static_cast<VkDeviceSize>(size);
    buffer_info.usage = static_cast<VkBufferUsageFlags>(usage);
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = memory_usage;
    alloc_info.flags = flags;

    VkResult result = vmaCreateBuffer(
        allocator, &buffer_info, &alloc_info,
        &buf.buffer_, &buf.allocation_, nullptr);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateBuffer failed");
    }

    return buf;
}

Buffer Buffer::create_vertex_buffer(
    const vk::raii::Device&      device,
    VmaAllocator                 allocator,
    const vk::raii::CommandPool& command_pool,
    const vk::raii::Queue&       queue,
    std::span<const std::byte>   data) {

    auto size = static_cast<vk::DeviceSize>(data.size());

    // Staging buffer (host-visible, mapped)
    Buffer staging = create(
        allocator, size,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void* mapped = staging.map();
    std::memcpy(mapped, data.data(), data.size());
    staging.unmap();

    // Device-local vertex buffer
    Buffer vertex_buf = create(
        allocator, size,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    // Copy via one-shot command buffer
    vk::CommandBufferAllocateInfo alloc_info{
        *command_pool,
        vk::CommandBufferLevel::ePrimary,
        1,
    };
    auto cmd_buffers = device.allocateCommandBuffers(alloc_info);
    auto& cmd = cmd_buffers[0];

    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd.copyBuffer(vk::Buffer{staging.buffer_}, vk::Buffer{vertex_buf.buffer_}, vk::BufferCopy{0, 0, size});
    cmd.end();

    vk::SubmitInfo submit_info{{}, {}, *cmd};
    queue.submit(submit_info);
    queue.waitIdle();

    return vertex_buf;
}

Buffer Buffer::create_index_buffer(
    const vk::raii::Device&      device,
    VmaAllocator                 allocator,
    const vk::raii::CommandPool& command_pool,
    const vk::raii::Queue&       queue,
    std::span<const std::byte>   data) {

    auto size = static_cast<vk::DeviceSize>(data.size());

    // Staging buffer (host-visible, mapped)
    Buffer staging = create(
        allocator, size,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void* mapped = staging.map();
    std::memcpy(mapped, data.data(), data.size());
    staging.unmap();

    // Device-local index buffer
    Buffer index_buf = create(
        allocator, size,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    // Copy via one-shot command buffer
    vk::CommandBufferAllocateInfo alloc_info{
        *command_pool,
        vk::CommandBufferLevel::ePrimary,
        1,
    };
    auto cmd_buffers = device.allocateCommandBuffers(alloc_info);
    auto& cmd = cmd_buffers[0];

    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd.copyBuffer(vk::Buffer{staging.buffer_}, vk::Buffer{index_buf.buffer_}, vk::BufferCopy{0, 0, size});
    cmd.end();

    vk::SubmitInfo submit_info{{}, {}, *cmd};
    queue.submit(submit_info);
    queue.waitIdle();

    return index_buf;
}

void* Buffer::map() {
    void* data = nullptr;
    vmaMapMemory(allocator_, allocation_, &data);
    return data;
}

void Buffer::unmap() {
    vmaUnmapMemory(allocator_, allocation_);
}

void* Buffer::mapped_data() const {
    VmaAllocationInfo info{};
    vmaGetAllocationInfo(allocator_, allocation_, &info);
    return info.pMappedData;
}

} // namespace steel

#pragma once

#include <steel/buffer.hpp>
#include <steel/engine.hpp>

#include <cstring>
#include <vector>

namespace steel {

template<typename T>
class UniformBuffer {
public:
    static UniformBuffer create(const Engine& engine, vk::ShaderStageFlags stages) {
        UniformBuffer ub;
        const auto& device = engine.device();
        const auto& physical_device = engine.physical_device();

        // Descriptor set layout
        vk::DescriptorSetLayoutBinding binding(
            0,
            vk::DescriptorType::eUniformBuffer,
            1,
            stages
        );
        vk::DescriptorSetLayoutCreateInfo layout_info({}, 1, &binding);
        ub.layout_ = vk::raii::DescriptorSetLayout(device, layout_info);

        // Descriptor pool
        vk::DescriptorPoolSize pool_size(
            vk::DescriptorType::eUniformBuffer,
            MAX_FRAMES_IN_FLIGHT
        );
        vk::DescriptorPoolCreateInfo pool_info({}, MAX_FRAMES_IN_FLIGHT, 1, &pool_size);
        ub.pool_ = vk::raii::DescriptorPool(device, pool_info);

        // Allocate descriptor sets
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *ub.layout_);
        vk::DescriptorSetAllocateInfo alloc_info(
            *ub.pool_,
            static_cast<uint32_t>(layouts.size()),
            layouts.data()
        );
        ub.sets_ = device.allocateDescriptorSets(alloc_info);

        // Create buffers, persistently map, write descriptor sets
        ub.buffers_.reserve(MAX_FRAMES_IN_FLIGHT);
        ub.mapped_.resize(MAX_FRAMES_IN_FLIGHT, nullptr);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            ub.buffers_.push_back(Buffer::create(
                device, physical_device,
                sizeof(T),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            ));

            ub.mapped_[i] = ub.buffers_[i].map();

            vk::DescriptorBufferInfo buffer_info(
                ub.buffers_[i].handle(), 0, sizeof(T)
            );
            vk::WriteDescriptorSet write(
                *ub.sets_[i],
                0, 0, 1,
                vk::DescriptorType::eUniformBuffer,
                nullptr,
                &buffer_info
            );
            device.updateDescriptorSets(write, nullptr);
        }

        return ub;
    }

    void update(uint32_t frame_index, const T& data) {
        std::memcpy(mapped_[frame_index], &data, sizeof(T));
    }

    void bind(const vk::raii::CommandBuffer& cmd,
              const vk::raii::PipelineLayout& layout,
              uint32_t set_index,
              uint32_t frame_index) const {
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *layout,
            set_index,
            *sets_[frame_index],
            {}
        );
    }

    const vk::raii::DescriptorSetLayout& layout() const { return layout_; }

    UniformBuffer() = default;
    UniformBuffer(UniformBuffer&&) = default;
    UniformBuffer& operator=(UniformBuffer&&) = default;

private:
    vk::raii::DescriptorSetLayout layout_{nullptr};
    vk::raii::DescriptorPool      pool_{nullptr};
    std::vector<vk::raii::DescriptorSet> sets_;
    std::vector<Buffer>                  buffers_;
    std::vector<void*>                   mapped_;
};

} // namespace steel

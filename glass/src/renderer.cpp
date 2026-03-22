#include <glass/renderer.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstring>

namespace glass {

Renderer::Renderer(steel::Engine& engine)
    : engine_(engine) {
    create_frame_descriptors();
}

void Renderer::create_frame_descriptors() {
    // 1. Create descriptor set layout
    vk::DescriptorSetLayoutBinding ubo_binding(
        0,                              // binding
        vk::DescriptorType::eUniformBuffer,
        1,                              // descriptorCount
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
    );

    vk::DescriptorSetLayoutCreateInfo layout_info(
        {},     // flags
        1,      // bindingCount
        &ubo_binding
    );
    frame_descriptor_layout_ = vk::raii::DescriptorSetLayout(engine_.device(), layout_info);

    // 2. Create descriptor pool
    vk::DescriptorPoolSize pool_size(
        vk::DescriptorType::eUniformBuffer,
        steel::MAX_FRAMES_IN_FLIGHT
    );

    vk::DescriptorPoolCreateInfo pool_info(
        {},                             // flags
        steel::MAX_FRAMES_IN_FLIGHT,    // maxSets
        1,                              // poolSizeCount
        &pool_size
    );
    frame_descriptor_pool_ = vk::raii::DescriptorPool(engine_.device(), pool_info);

    // 3. Allocate descriptor sets
    std::vector<vk::DescriptorSetLayout> layouts(steel::MAX_FRAMES_IN_FLIGHT, *frame_descriptor_layout_);
    vk::DescriptorSetAllocateInfo alloc_info(
        *frame_descriptor_pool_,
        static_cast<uint32_t>(layouts.size()),
        layouts.data()
    );
    frame_descriptor_sets_ = engine_.device().allocateDescriptorSets(alloc_info);

    // 4. Create UBO buffers and persistently map them
    frame_ubo_buffers_.reserve(steel::MAX_FRAMES_IN_FLIGHT);
    frame_ubo_mapped_.resize(steel::MAX_FRAMES_IN_FLIGHT, nullptr);

    for (uint32_t i = 0; i < steel::MAX_FRAMES_IN_FLIGHT; ++i) {
        frame_ubo_buffers_.push_back(steel::Buffer::create(
            engine_.device(),
            engine_.physical_device(),
            sizeof(FrameUBO),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        ));

        frame_ubo_mapped_[i] = frame_ubo_buffers_[i].map();

        // 5. Write descriptor set
        vk::DescriptorBufferInfo buffer_info(
            frame_ubo_buffers_[i].handle(),
            0,
            sizeof(FrameUBO)
        );

        vk::WriteDescriptorSet write(
            *frame_descriptor_sets_[i],     // dstSet
            0,                              // dstBinding
            0,                              // dstArrayElement
            1,                              // descriptorCount
            vk::DescriptorType::eUniformBuffer,
            nullptr,                        // pImageInfo
            &buffer_info                    // pBufferInfo
        );

        engine_.device().updateDescriptorSets(write, nullptr);
    }
}

void Renderer::update_frame_ubo(uint32_t frame_index, const glm::mat4& view_projection) {
    FrameUBO ubo{};
    ubo.view_projection = view_projection;
    std::memcpy(frame_ubo_mapped_[frame_index], &ubo, sizeof(FrameUBO));
}

void Renderer::run(World& world) {
    while (engine_.poll_events()) {
        render_frame(world);
    }
    engine_.wait_idle();
}

void Renderer::render_frame(World& world) {
    // Find active camera and compute view_projection from Transform
    glm::mat4 view_projection{1.0f};
    bool found_camera = false;

    world.view<Transform, CameraComponent>().each([&](Entity e, Transform& t, CameraComponent& cc) {
        if (cc.active) {
            auto extent = engine_.extent();
            float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
            cc.camera.set_aspect_ratio(aspect);

            // View matrix is the inverse of the camera's world transform
            glm::mat4 view = glm::inverse(t.matrix);
            view_projection = cc.camera.projection() * view;
            found_camera = true;
        }
    });

    if (!found_camera) {
        return;
    }

    auto* cmd = engine_.begin_frame();
    if (cmd) {
        update_frame_ubo(engine_.current_frame(), view_projection);
        render_ecs(*cmd, world, engine_.current_frame());
        engine_.end_frame();
    }
}

void Renderer::render_ecs(const vk::raii::CommandBuffer& cmd,
                          World& world,
                          uint32_t frame_index) const {
    world.view<Transform, MeshComponent, MaterialComponent>()
        .each([&](Entity e, Transform& t, MeshComponent& mesh, MaterialComponent& mat) {
            mat.material->bind(cmd);
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *mat.material->layout(),
                0,
                *frame_descriptor_sets_[frame_index],
                {}
            );
            cmd.pushConstants<glm::mat4>(
                *mat.material->layout(),
                vk::ShaderStageFlagBits::eVertex,
                0,
                t.matrix);
            mesh.geometry->bind(cmd);
            mesh.geometry->draw(cmd);
        });
}

} // namespace glass

#include <steel/imgui_pass.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <spdlog/spdlog.h>

#include <array>

namespace steel {

ImGuiPass::~ImGuiPass() {
    shutdown();
}

void ImGuiPass::create(const vk::raii::Instance& instance,
                       const vk::raii::PhysicalDevice& physical_device,
                       const vk::raii::Device& device,
                       uint32_t graphics_family_index,
                       const vk::raii::Queue& graphics_queue,
                       vk::Format surface_format,
                       vk::Extent2D extent,
                       const std::vector<vk::raii::ImageView>& swapchain_image_views,
                       uint32_t swapchain_image_count,
                       SDL_Window* window) {
    create_render_pass(device, surface_format);
    create_framebuffers(device, extent, swapchain_image_views);

    // Create a dedicated descriptor pool for ImGui
    std::array<vk::DescriptorPoolSize, 1> pool_sizes = {{
        {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1},
    }};

    vk::DescriptorPoolCreateInfo pool_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    descriptor_pool_ = vk::raii::DescriptorPool{device, pool_info};

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Initialize SDL3 backend
    ImGui_ImplSDL3_InitForVulkan(window);

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = *instance;
    init_info.PhysicalDevice = *physical_device;
    init_info.Device = *device;
    init_info.QueueFamily = graphics_family_index;
    init_info.Queue = *graphics_queue;
    init_info.DescriptorPool = *descriptor_pool_;
    init_info.PipelineInfoMain.RenderPass = *render_pass_;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.MinImageCount = 2;
    init_info.ImageCount = swapchain_image_count;

    ImGui_ImplVulkan_Init(&init_info);

    initialized_ = true;
    spdlog::info("ImGui initialized");
}

void ImGuiPass::shutdown() {
    if (!initialized_) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void ImGuiPass::begin() {
    if (!initialized_) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiPass::end() {
    if (!initialized_) return;

    ImGui::Render();
}

void ImGuiPass::render(const vk::raii::CommandBuffer& cmd,
                       uint32_t image_index,
                       vk::Extent2D extent) const {
    if (!initialized_) return;

    vk::RenderPassBeginInfo imgui_rp_info{
        .renderPass = *render_pass_,
        .framebuffer = *framebuffers_[image_index],
        .renderArea = vk::Rect2D{.offset = {0, 0}, .extent = extent},
    };

    cmd.beginRenderPass(imgui_rp_info, vk::SubpassContents::eInline);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);
    cmd.endRenderPass();
}

void ImGuiPass::process_event(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void ImGuiPass::recreate(const vk::raii::Device& device,
                         vk::Extent2D extent,
                         const std::vector<vk::raii::ImageView>& swapchain_image_views) {
    framebuffers_.clear();
    create_framebuffers(device, extent, swapchain_image_views);
}

void ImGuiPass::create_render_pass(const vk::raii::Device& device, vk::Format surface_format) {
    vk::AttachmentDescription color_attachment{
        .format = surface_format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentReference color_ref{.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
    };

    vk::RenderPassCreateInfo create_info{
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    render_pass_ = vk::raii::RenderPass{device, create_info};
    spdlog::info("ImGui render pass created");
}

void ImGuiPass::create_framebuffers(const vk::raii::Device& device,
                                    vk::Extent2D extent,
                                    const std::vector<vk::raii::ImageView>& swapchain_image_views) {
    framebuffers_.clear();
    for (const auto& image_view : swapchain_image_views) {
        vk::ImageView attachment = *image_view;

        vk::FramebufferCreateInfo create_info{
            .renderPass = *render_pass_,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        framebuffers_.emplace_back(device, create_info);
    }
}

} // namespace steel

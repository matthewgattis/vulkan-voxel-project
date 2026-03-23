#include <steel/swapchain.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace steel {

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void Swapchain::create(const vk::raii::PhysicalDevice& physical_device,
                       const vk::raii::Device& device,
                       const vk::raii::SurfaceKHR& surface,
                       VmaAllocator allocator,
                       SDL_Window* window,
                       uint32_t graphics_family_index,
                       uint32_t present_family_index) {
    depth_format_ = find_depth_format(physical_device);
    create_swapchain(physical_device, device, surface, window,
                     graphics_family_index, present_family_index);
    create_depth_resources(device, allocator);
    create_offscreen_target(device, allocator);
    create_render_pass(device);
    create_offscreen_framebuffer(device);
}

void Swapchain::recreate(const vk::raii::PhysicalDevice& physical_device,
                         const vk::raii::Device& device,
                         const vk::raii::SurfaceKHR& surface,
                         VmaAllocator allocator,
                         SDL_Window* window,
                         uint32_t graphics_family_index,
                         uint32_t present_family_index) {
    // Destroy extent-dependent resources in reverse creation order
    offscreen_framebuffer_ = vk::raii::Framebuffer{nullptr};
    depth_image_view_ = vk::raii::ImageView{nullptr};
    depth_image_ = VmaImage{};
    offscreen_image_view_ = vk::raii::ImageView{nullptr};
    offscreen_image_ = VmaImage{};
    image_views_.clear();

    create_swapchain(physical_device, device, surface, window,
                     graphics_family_index, present_family_index);
    create_depth_resources(device, allocator);
    create_offscreen_target(device, allocator);
    create_offscreen_framebuffer(device);

    spdlog::info("Swapchain recreated ({}x{})", extent_.width, extent_.height);
}

// ---------------------------------------------------------------------------
// Swapchain
// ---------------------------------------------------------------------------

void Swapchain::create_swapchain(const vk::raii::PhysicalDevice& physical_device,
                                 const vk::raii::Device& device,
                                 const vk::raii::SurfaceKHR& surface,
                                 SDL_Window* window,
                                 uint32_t graphics_family_index,
                                 uint32_t present_family_index) {
    auto capabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    auto formats      = physical_device.getSurfaceFormatsKHR(*surface);

    // Pick surface format: prefer B8G8R8A8_SRGB
    surface_format_ = formats[0];
    for (const auto& fmt : formats) {
        if (fmt.format == vk::Format::eB8G8R8A8Srgb &&
            fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surface_format_ = fmt;
            break;
        }
    }

    // Extent
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent_ = capabilities.currentExtent;
    } else {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        extent_.width  = std::clamp(static_cast<uint32_t>(w),
            capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
        extent_.height = std::clamp(static_cast<uint32_t>(h),
            capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    vk::SwapchainKHR old_swapchain = *swapchain_;

    vk::SwapchainCreateInfoKHR create_info{
        .surface = *surface,
        .minImageCount = image_count,
        .imageFormat = surface_format_.format,
        .imageColorSpace = surface_format_.colorSpace,
        .imageExtent = extent_,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eFifo,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    if (graphics_family_index != present_family_index) {
        std::array<uint32_t, 2> indices = {graphics_family_index, present_family_index};
        create_info.imageSharingMode      = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = static_cast<uint32_t>(indices.size());
        create_info.pQueueFamilyIndices   = indices.data();
    }

    swapchain_ = vk::raii::SwapchainKHR{device, create_info};
    images_ = swapchain_.getImages();

    // Create image views
    image_views_.clear();
    for (auto image : images_) {
        vk::ImageViewCreateInfo view_info{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = surface_format_.format,
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
        };
        image_views_.emplace_back(device, view_info);
    }

    spdlog::info("Swapchain created ({}x{}, {} images, format {})",
                 extent_.width, extent_.height,
                 images_.size(), vk::to_string(surface_format_.format));
}

// ---------------------------------------------------------------------------
// Depth buffer
// ---------------------------------------------------------------------------

void Swapchain::create_depth_resources(const vk::raii::Device& device, VmaAllocator allocator) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = static_cast<VkFormat>(depth_format_);
    image_info.extent = {extent_.width, extent_.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    depth_image_ = VmaImage{};
    depth_image_.allocator = allocator;
    VkResult result = vmaCreateImage(allocator, &image_info, &alloc_info,
                                     &depth_image_.image, &depth_image_.allocation, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage failed for depth buffer");
    }

    vk::ImageViewCreateInfo view_info{
        .image = vk::Image{depth_image_.image},
        .viewType = vk::ImageViewType::e2D,
        .format = depth_format_,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
    };

    depth_image_view_ = vk::raii::ImageView{device, view_info};
    spdlog::info("Depth buffer created (format {})", vk::to_string(depth_format_));
}

// ---------------------------------------------------------------------------
// Offscreen render target
// ---------------------------------------------------------------------------

void Swapchain::create_offscreen_target(const vk::raii::Device& device, VmaAllocator allocator) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = static_cast<VkFormat>(surface_format_.format);
    image_info.extent = {extent_.width, extent_.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    offscreen_image_ = VmaImage{};
    offscreen_image_.allocator = allocator;
    VkResult result = vmaCreateImage(allocator, &image_info, &alloc_info,
                                     &offscreen_image_.image, &offscreen_image_.allocation, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage failed for offscreen target");
    }

    vk::ImageViewCreateInfo view_info{
        .image = vk::Image{offscreen_image_.image},
        .viewType = vk::ImageViewType::e2D,
        .format = surface_format_.format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
    };

    offscreen_image_view_ = vk::raii::ImageView{device, view_info};
    spdlog::info("Offscreen render target created ({}x{})", extent_.width, extent_.height);
}

// ---------------------------------------------------------------------------
// Render pass (scene -- outputs to shader-read-only for FXAA)
// ---------------------------------------------------------------------------

void Swapchain::create_render_pass(const vk::raii::Device& device) {
    vk::AttachmentDescription color_attachment{
        .format = surface_format_.format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::AttachmentDescription depth_attachment{
        .format = depth_format_,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    vk::AttachmentReference color_ref{.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};
    vk::AttachmentReference depth_ref{.attachment = 1, .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL, .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
    };

    std::array<vk::AttachmentDescription, 2> attachments = {color_attachment, depth_attachment};

    vk::RenderPassCreateInfo create_info{
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    render_pass_ = vk::raii::RenderPass{device, create_info};
}

// ---------------------------------------------------------------------------
// Offscreen framebuffer (scene pass)
// ---------------------------------------------------------------------------

void Swapchain::create_offscreen_framebuffer(const vk::raii::Device& device) {
    std::array<vk::ImageView, 2> attachments = {*offscreen_image_view_, *depth_image_view_};

    vk::FramebufferCreateInfo create_info{
        .renderPass = *render_pass_,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = extent_.width,
        .height = extent_.height,
        .layers = 1,
    };

    offscreen_framebuffer_ = vk::raii::Framebuffer{device, create_info};
}

// ---------------------------------------------------------------------------
// Depth format selection
// ---------------------------------------------------------------------------

vk::Format Swapchain::find_depth_format(const vk::raii::PhysicalDevice& dev) const {
    constexpr std::array candidates = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
    };

    for (auto format : candidates) {
        auto props = dev.getFormatProperties(format);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    return vk::Format::eUndefined;
}

} // namespace steel

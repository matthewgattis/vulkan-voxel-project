#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>

#include <vector>

namespace steel {

// Wraps VkImage + VmaAllocation with RAII cleanup.
struct VmaImage {
    VmaAllocator  allocator  = VK_NULL_HANDLE;
    VkImage       image      = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    VmaImage() = default;
    ~VmaImage() { destroy(); }
    VmaImage(VmaImage&& o) noexcept
        : allocator{o.allocator}, image{o.image}, allocation{o.allocation}
    { o.image = VK_NULL_HANDLE; o.allocation = VK_NULL_HANDLE; }
    VmaImage& operator=(VmaImage&& o) noexcept {
        if (this != &o) {
            destroy();
            allocator = o.allocator; image = o.image; allocation = o.allocation;
            o.image = VK_NULL_HANDLE; o.allocation = VK_NULL_HANDLE;
        }
        return *this;
    }
    VmaImage(const VmaImage&) = delete;
    VmaImage& operator=(const VmaImage&) = delete;
private:
    void destroy() { if (image) { vmaDestroyImage(allocator, image, allocation); image = VK_NULL_HANDLE; } }
};

class Swapchain {
public:
    // Create the swapchain and all associated resources.
    void create(const vk::raii::PhysicalDevice& physical_device,
                const vk::raii::Device& device,
                const vk::raii::SurfaceKHR& surface,
                VmaAllocator allocator,
                SDL_Window* window,
                uint32_t graphics_family_index,
                uint32_t present_family_index);

    // Recreate after resize. Waits for device idle, destroys old resources, creates new ones.
    void recreate(const vk::raii::PhysicalDevice& physical_device,
                  const vk::raii::Device& device,
                  const vk::raii::SurfaceKHR& surface,
                  VmaAllocator allocator,
                  SDL_Window* window,
                  uint32_t graphics_family_index,
                  uint32_t present_family_index);

    // Accessors
    const vk::raii::SwapchainKHR& handle() const { return swapchain_; }
    const vk::raii::RenderPass& render_pass() const { return render_pass_; }
    vk::Extent2D extent() const { return extent_; }
    vk::Format color_format() const { return surface_format_.format; }
    vk::Format depth_format() const { return depth_format_; }
    const std::vector<vk::Image>& images() const { return images_; }
    const std::vector<vk::raii::ImageView>& image_views() const { return image_views_; }
    const vk::raii::ImageView& offscreen_image_view() const { return offscreen_image_view_; }
    const vk::raii::Framebuffer& offscreen_framebuffer() const { return offscreen_framebuffer_; }

    Swapchain() = default;
    Swapchain(Swapchain&&) = default;
    Swapchain& operator=(Swapchain&&) = default;

private:
    void create_swapchain(const vk::raii::PhysicalDevice& physical_device,
                          const vk::raii::Device& device,
                          const vk::raii::SurfaceKHR& surface,
                          SDL_Window* window,
                          uint32_t graphics_family_index,
                          uint32_t present_family_index);
    void create_depth_resources(const vk::raii::Device& device, VmaAllocator allocator);
    void create_offscreen_target(const vk::raii::Device& device, VmaAllocator allocator);
    void create_render_pass(const vk::raii::Device& device);
    void create_offscreen_framebuffer(const vk::raii::Device& device);

    vk::Format find_depth_format(const vk::raii::PhysicalDevice& dev) const;

    // Swapchain
    vk::SurfaceFormatKHR surface_format_;
    vk::Extent2D extent_;
    vk::Format depth_format_ = vk::Format::eD32Sfloat;
    vk::raii::SwapchainKHR swapchain_{nullptr};
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> image_views_;

    // Depth buffer
    VmaImage depth_image_;
    vk::raii::ImageView depth_image_view_{nullptr};

    // Offscreen render target
    VmaImage offscreen_image_;
    vk::raii::ImageView offscreen_image_view_{nullptr};

    // Scene render pass & framebuffer
    vk::raii::RenderPass render_pass_{nullptr};
    vk::raii::Framebuffer offscreen_framebuffer_{nullptr};
};

} // namespace steel

#pragma once
#include <vulkan/vulkan.h>
#include "preprocessor.hpp"

// convenience struct for creating a swapchain that complies with the surface requirements.
// the structure also contains all the resolved swapchain information such as the selected format and extent
class Swapchain
{
public:
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkSurfaceCapabilitiesKHR capabilities;
    
    VkExtent2D extent;
    uint32_t imageCount;
    VkFormat format;
    VkColorSpaceKHR colorSpace;
    
    static class Swapchain create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, int32_t graphicsFamily, int32_t presentFamily)
    {
        class Swapchain result;
        result.surface = surface;
        
        // Get the surface capabilities to figure out the surface's
        // limits such as its min/max extent, image count, etc.
        THROW_IF_FAILED(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, result.surface, &result.capabilities));
        
        if (!result.supported())
            return {};
        
        result.selectExtent();
        result.selectImageCount();
        result.selectFormat(physicalDevice, surface);
        
        // a swapchain swaps images between the presentation engine and the application
        // this way, we can work on rendering to one image, while the other is being read by a screen
        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.pNext = nullptr;
        swapchainInfo.flags = 0;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = result.imageCount;
        swapchainInfo.imageFormat = result.format;
        swapchainInfo.imageColorSpace = result.colorSpace;
        swapchainInfo.imageExtent = result.extent;
        swapchainInfo.imageArrayLayers = 1; // not relevant
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // do nothing to the transform
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR; // default
        swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // always supported, vsync enabled swapchain
        swapchainInfo.clipped = false; // not relevant
        swapchainInfo.oldSwapchain = nullptr; // not relevant
        
        // resources such as a swapchain need to know what queue family(s) they'll be used in
        // if present and graphics are the same then we should make the sharing mode exclusive for potentially enhanced performance.
        std::array<uint32_t, 2> families { static_cast<uint32_t>(presentFamily), static_cast<uint32_t>(graphicsFamily) };
        if (presentFamily != graphicsFamily)
        {
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainInfo.queueFamilyIndexCount = families.size();
            swapchainInfo.pQueueFamilyIndices = families.data();
        }
        else{
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchainInfo.queueFamilyIndexCount = 0; // optional
            swapchainInfo.pQueueFamilyIndices = nullptr; // optional
        }
        
        THROW_IF_FAILED(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &result.swapchain));
        
        return result;
    }
    
    std::vector<VkImage>& getImages(VkDevice device)
    {
        if (!m_images.empty())
            return m_images;
        
        // get the VkImages from our swapchain
        // these images are what we'll be rendering to
        uint32_t count;
        vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
        std::vector<VkImage> swapchainImages(count);
        vkGetSwapchainImagesKHR(device, swapchain, &count, swapchainImages.data());
        
        m_images = swapchainImages;
        return m_images;
    }
    
    std::vector<VkImageView>& getImageViews(VkDevice device)
    {
        if (!m_imageViews.empty())
            return m_imageViews;
        
        m_imageViews = std::vector<VkImageView>(m_images.size());

        for (size_t i = 0; i < m_images.size(); i++)
        {
            // use identity component mapping (nothing changes)
            VkComponentMapping mapping;
            mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // a subresource range describes what parts of the image are affected by something
            // this way you can make it affect certain mip levels or array layers
            // our swapchain images are simple 2D images without mipmaps and without array layers
            VkImageSubresourceRange swapchainSubresourceRange {};
            swapchainSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            swapchainSubresourceRange.baseMipLevel = 0;
            swapchainSubresourceRange.levelCount = 1;
            swapchainSubresourceRange.baseArrayLayer = 0;
            swapchainSubresourceRange.layerCount = 1;
            
            // an image view describes how an image is used/referenced by the GPU
            VkImageViewCreateInfo viewInfo {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.pNext = nullptr;
            viewInfo.flags = 0;
            viewInfo.image = m_images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.components = mapping;
            viewInfo.subresourceRange = swapchainSubresourceRange;
            
            THROW_IF_FAILED(vkCreateImageView(device, &viewInfo, nullptr, &m_imageViews[i]));
        }
        
        return m_imageViews;
    }
    
    std::vector<VkFramebuffer> getFramebuffers(VkDevice device, VkRenderPass renderpass)
    {
        auto& views = getImageViews(device);
        
        // Creating a framebuffer for the swapchain images is necessary to be able to render to them using our renderpass
        // the image view is required for framebuffer creation
        std::vector<VkFramebuffer> framebuffers(m_images.size());
        for (size_t i = 0; i < m_images.size(); i++)
        {
            // a framebuffer is an image that can be used by a renderpass
            // the renderpass can write to this image or change its layout
            VkFramebufferCreateInfo framebufferInfo {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.pNext = nullptr;
            framebufferInfo.flags = 0;
            framebufferInfo.renderPass = renderpass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &views[i];
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;
            
            THROW_IF_FAILED(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]));
        }
        
        return framebuffers;
    }
    
private:
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    bool supported()
    {
        // we also need to check if we can use the surface's images as a color attachment
        // this is needed so we can draw to it, but if it isn't supported we could
        // draw to a different image and copy to the swapchain images instead
        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
            printf("Surface doesn't support IMAGE_USAGE_COLOR_ATTACHMENT_BIT");
            return false;
        }
        
        // must support transfer dst for clearing the image
        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        {
            printf("Surface doesn't support IMAGE_USAGE_TRANSFER_DST_BIT (required for clear image)");
            return false;
        }
        
        return true;
    }
    
    void selectExtent()
    {
        // clamp our selected window size to the min/max surface extent
        extent.width = std::clamp(800u, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(800u, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    
    void selectImageCount()
    {
        // clamp our desired image count (we'll pick 2 for now) and clamp between min/max image count
        imageCount = std::clamp(2u, capabilities.minImageCount, capabilities.maxImageCount);
    }
    
    void selectFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
    {
        // iterate the available surface formats and pick a format
        uint32_t count;
        THROW_IF_FAILED(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr));
        std::vector<VkSurfaceFormatKHR> surfaceFormats(count);
        THROW_IF_FAILED(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, surfaceFormats.data()));
        
        VkSurfaceFormatKHR selectedFormat = surfaceFormats[0]; // fallback format
        for (const auto& f : surfaceFormats)
        {
            // ideally we find an sRGB format for better color accuracy
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                format = f.format;
                colorSpace = f.colorSpace;
            }
        }
    }
};

#pragma once
#include <vulkan/vulkan.h>

class QueueFamilies
{
public:
    // note that these families may end up being the same family
    int32_t graphics = -1; // capable of rasterization graphics
    int32_t present = -1; // capable of presenting to a surface
    
    bool valid() { return graphics != -1 && present != -1; }
    bool exclusive() { return graphics == present; }
    
    static QueueFamilies select(VkInstance instance, VkPhysicalDevice pd, VkSurfaceKHR surface)
    {
        QueueFamilies families;
        
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, queueFamilyProperties.data());
        
        // A physical device can have multiple queue families that correspond to different/combined parts of the GPU.
        // Higher end NVIDIA GPUs for example often have a general graphics/compute/transfer family,
        // a dedicated compute family, and a dedicated transfer family.
        // Dedicated families may perform better and may run in parallel with other
        // families (e.g. a dedicated transfer family might operate directly through the gpu's memory controller)
        for (size_t i = 0; i < count; i++)
        {
            // find a graphics family
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
                families.graphics = i;
            
            // make sure we can present to the surface with this family
            bool presentationSupport = glfwGetPhysicalDevicePresentationSupport(instance, pd, i);
            
            uint32_t surfaceSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &surfaceSupport);
            if (presentationSupport && surfaceSupport)
                families.present = i;
        }
        
        return families;
    }
    
private:
};

#pragma once
#include <vulkan/vulkan.h>
#include "queue_families.hpp"

class PhysicalDevice
{
public:
    // selects a physical device
    // picks the first one that supports our needs
    static VkPhysicalDevice select(VkInstance instance, VkSurfaceKHR surface, QueueFamilies* outQueueFamilies)
    {
        // get all available physical devices
        uint32_t count;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(count);
        vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());
        
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        for (auto pd : physicalDevices)
        {
            QueueFamilies families = QueueFamilies::select(instance, pd, surface);
            
            if (!families.valid())
                continue;
            
            Extensions extensions { pd };
            if (!extensions.available("VK_KHR_swapchain"))
                continue;
            
            *outQueueFamilies = families;
            physicalDevice = pd;
        }
        
        assert(physicalDevice != nullptr);
        return physicalDevice;
    }
};

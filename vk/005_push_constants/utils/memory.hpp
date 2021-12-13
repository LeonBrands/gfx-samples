#pragma once
#include <vulkan/vulkan.h>

class Memory
{
public:
    static uint32_t select(VkPhysicalDevice physicalDevice, VkMemoryRequirements memoryReqs, VkMemoryPropertyFlags flags)
    {
        // Before we start allocating memory, we should first query the physical device's memory properties.
        // when allocating memory, we must select a compatible memory type
        // our buffer will have a certain set of requirements, and we may have requirements or desires ourselves too
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        
        // using the given memory requirements and the previously acquired physical device memory properties
        // we can select a memory type index that is appropriate for our buffer's memory
        int32_t index = -1;
        for (size_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        {
            auto memoryType = memoryProperties.memoryTypes[i];
            
            // we'll select a host-coherent/visible type here
            // being host (cpu) visible is not ideal for buffers and textures -
            // ideally we create a separate buffer that is device_local and
            // then we do a gpu-gpu copy to the said buffer
            
            if ((memoryType.propertyFlags & flags) != flags)
                continue;
            
            // the memory requirements must also match with the memory we're selecting
            if((memoryType.propertyFlags & memoryReqs.memoryTypeBits) == memoryReqs.memoryTypeBits)
                index = i;
        }
        
        assert(index != -1);
        return index;
    }
};

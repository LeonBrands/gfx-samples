#pragma once
#include <vulkan/vulkan.h>
#include "queue_families.hpp"

// wrapper around vulkan buffer creation/destruction, exposes VkBuffer and VkMemory
// static creation functions wrap around different kinds of functionality
class Buffer
{
public:
    Buffer() = default;
    ~Buffer() {
        vkDestroyBuffer(m_device, buffer, nullptr);
        vkFreeMemory(m_device, memory, nullptr);
    }
    
    // create an upload buffer and copy the data to the buffer's memory
    // upload buffers might not be optimal for performance but they allow us to upload data to the GPU
    static std::unique_ptr<Buffer> createUploadBuffer(VkDevice device, VkPhysicalDevice physicalDevice, const QueueFamilies& families, uint32_t sizeInBytes, void* data, VkBufferUsageFlags usage)
    {
        std::unique_ptr<Buffer> result = std::make_unique<Buffer>();
        result->m_device = device;
        
        // Describe our buffer's size and usage
        // and similar to VkSwapchainKHR, we must describe what queue families get access to it
        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;
        bufferInfo.flags = 0;
        bufferInfo.size = sizeInBytes;
        bufferInfo.usage = usage;
        
        std::array<uint32_t, 2> familyArr { static_cast<uint32_t>(families.present), static_cast<uint32_t>(families.graphics) };
        if (families.present != families.graphics)
        {
            bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            bufferInfo.queueFamilyIndexCount = familyArr.size();
            bufferInfo.pQueueFamilyIndices = familyArr.data();
        }
        else{
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            bufferInfo.queueFamilyIndexCount = 0; // optional
            bufferInfo.pQueueFamilyIndices = nullptr; // optional
        }
        
        THROW_IF_FAILED(vkCreateBuffer(device, &bufferInfo, nullptr, &result->buffer));
        
        // After creating the buffer, we need to request its memory requirements.
        // This will help us determine how much (and what kind of) memory we'll need to allocate for it
        VkMemoryRequirements memoryReqs;
        vkGetBufferMemoryRequirements(device, result->buffer, &memoryReqs);
        uint32_t index = Memory::select(physicalDevice, memoryReqs, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        // describe how the memory should be allocated
        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.allocationSize = memoryReqs.size;
        allocInfo.memoryTypeIndex = index;
        
        THROW_IF_FAILED(vkAllocateMemory(device, &allocInfo, nullptr, &result->memory));

        // finally, bind the buffer and its memory
        THROW_IF_FAILED(vkBindBufferMemory(device, result->buffer, result->memory, 0));
        
        // copy data to our buffer
        void* ptr;
        THROW_IF_FAILED(vkMapMemory(device, result->memory, 0, sizeInBytes, 0, &ptr));
        memcpy(ptr, data, sizeInBytes);
        vkUnmapMemory(device, result->memory);
        
        return std::move(result);
    }
    
    VkBuffer buffer;
    VkDeviceMemory memory;
    
private:
    
    VkDevice m_device;
};

#pragma once
#include <vulkan/vulkan.h>
#include <set>

// convenience class for getting our requested set of vulkan layers
class Layers
{
public:
    static std::vector<const char*> get()
    {
        // vulkan layers intercept vulkan API calls to perform all kinds of checks
        // they may for example validate the corectness of your usage of the API,
        // or they could give suggestions for platform/device-specific performance improvements
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> supportedInstanceLayers(count);
        vkEnumerateInstanceLayerProperties(&count, supportedInstanceLayers.data());
        
        std::vector<const char*> layers{};
#ifndef NDEBUG
        // layers do come at a CPU runtime cost so it is usually not recommended to enable them in release builds
        // we'll enable the VK_LAYER_KHRONOS_validation layer here, which validates the corectness of API usage
        if (std::find_if(supportedInstanceLayers.begin(), supportedInstanceLayers.end(), [](auto item) { return strcmp(item.layerName, "VK_LAYER_KHRONOS_validation") == 0; } ) != supportedInstanceLayers.end())
            layers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif
        
        return layers;
    }
};

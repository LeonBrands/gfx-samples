#pragma once
#include <vulkan/vulkan.h>
#include "preprocessor.hpp"

class Shader
{
public:
    static VkShaderModule load(VkDevice device, std::string path)
    {
        // shaders are compiled from glsl to spirv using a compiler (e.g. glslc)
        // spirv is a binary format that we'll reeed in as a char (uint8_t) array
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        
        size_t size = (size_t) file.tellg();
        std::vector<char> fileBuffer(size);
        file.seekg(0);
        file.read(fileBuffer.data(), size);
        file.close();
        
        // pass the shader data on to the drivers through a "VkShaderModule"
        VkShaderModuleCreateInfo moduleInfo {};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.pNext = nullptr;
        moduleInfo.flags = 0;
        moduleInfo.codeSize = fileBuffer.size();
        moduleInfo.pCode = reinterpret_cast<uint32_t*>(fileBuffer.data());
        
        VkShaderModule shaderModule;
        THROW_IF_FAILED(vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule));
        
        return shaderModule;
    }
};

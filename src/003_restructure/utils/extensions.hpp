#pragma once
#include <vulkan/vulkan.h>
#include <set>

// convenience class for checking against available extensions
// and for collecting enabled extensions
class Extensions
{
public:
    // default extensions structure uses VkInstance extensions
    // upon creation, collect the extensions so we can easily compare with them
    Extensions()
    {
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> supportedInstanceExtensions(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, supportedInstanceExtensions.data());
        
        for (auto ext : supportedInstanceExtensions)
            m_available.insert(std::string(ext.extensionName));
    }
    
    // physical device can be passed to check for device extensions instead
    Extensions(VkPhysicalDevice physicalDevice)
    {
        uint32_t count;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> supportedDeviceExtensions(count);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, supportedDeviceExtensions.data());
        
        for (auto ext : supportedDeviceExtensions)
            m_available.insert(std::string(ext.extensionName));
    }
    
    // returns true if the extension is supported
    bool available(const char* extensionName)
    {
        return m_available.find(extensionName) != m_available.end();
    }
    
    // returns true if the extension has been added - through add() or addRequiredGLFW()
    bool enabled(const char* extensionName)
    {
        return m_enabled.find(extensionName) != m_enabled.end();
    }
    
    // convenient GLFW instance extension function
    // collects and adds the required GLFW extensions
    bool addRequiredGLFW()
    {
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        add(glfwExtensions, glfwExtensionCount, true);
        return true;
    }
    
    // add an extension to the enabled extension list
    // Returns true if the extension was added successfully, and false if it wasn't supported.
    // if throwIfNotSupported is true, the function throws if the extension is not supported
    bool add(const char* extensionName, bool throwIfNotSupported = false)
    {
        if (!available(extensionName))
        {
            if (throwIfNotSupported)
            {
                printf("Failed to load required extension %s\n", extensionName);
                throw std::runtime_error("Failed to load required extension");
            }
            
            return false;
        }
        
        m_enabled.insert(extensionName);
        return true;
    }
    
    // add multiple extensions to the enabled extension list
    // this returns a vector of size count, filled with boolean results of individual add()s.
    // if throwIfNotSupported is true, this function will throw upon the first unsupported extension
    std::vector<bool> add(const char** extensionNames, size_t count, bool throwIfNotSupported = false)
    {
        std::vector<bool> results(count);
        
        for (size_t i = 0; i < count; i++)
        {
            results[i] = add(extensionNames[i], throwIfNotSupported);
        }
        
        return results;
    }
    
    // return the enabled extensions as a vector, ready to be passed to a createinfo struct
    std::vector<const char*> get()
    {
        return std::vector<const char*>(m_enabled.begin(), m_enabled.end());
    }
    
private:
    std::set<std::string> m_available;
    std::set<const char*> m_enabled;
};

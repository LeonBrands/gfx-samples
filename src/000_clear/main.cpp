#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <map>
#include <array>
#include <vector>

int main() {
    // default GLFW window creation except we disable OpenGL context creation
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 800, "VkLectureSamples", nullptr, nullptr);

    uint32_t count; // convenience variable used for vkEnumerate calls
    
    // get vulkan supported extensions
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> supportedInstanceExtensions(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, supportedInstanceExtensions.data());

    // the sample needs two extensions: VK_KHR_surface and a platform-dependent VK_KHR_xxx_surface
    // this allows us to render to a native surface/window. we'll let GLFW handle this in this sample
    uint32_t extensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
    std::vector<const char*> extensions = std::vector<const char*>(glfwExtensions, glfwExtensions + extensionCount);

#ifdef __APPLE__
    // not covered in the presentation, but necessary for Apple implementations
    // because MoltenVK lacks some Vulkan 1.0 features that might need manual querying
    extensions.push_back("VK_KHR_get_physical_device_properties2");
#endif
    
    for (size_t i = 0; i < extensions.size(); i++)
    {
        // there are many ways to abstract away instance extension handling but
        // here we'll just use find_if
        if (std::find_if(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), [=](auto item) { return strcmp(item.extensionName, extensions[i]); }) == supportedInstanceExtensions.end())
        {
            // note that it's not uncommon to request extensions that you consider optional, renderers often enable/disable features based on extension availability.
            // this sample will just require the ability to render to a native surface and return -1
            // if it can't.
            printf("Vulkan does not support a required extension");
            return -1;
        }
    }
    
    // vulkan layers intercept vulkan API calls to perform all kinds of checks
    // they may for example validate the corectness of your usage of the API,
    // or they could give suggestions for platform/device-specific performance improvements
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
    
    // VkApplicationInfo is largely informative and usually just gives drivers additional information
    // for debugging purposes.
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "VkLectureSamples";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "None";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    // api version is the exception to this; changing the apiVersion changes which Vulkan API version is used.
    // newer API versions usually integrate popular extensions into the core.
    appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);
    
    VkInstanceCreateInfo instanceInfo {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = nullptr;
    instanceInfo.flags = 0;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledLayerCount = layers.size();
    instanceInfo.ppEnabledLayerNames = layers.data();
    instanceInfo.enabledExtensionCount = extensions.size();
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    
    // create a vulkan instance using the instance create info
    VkInstance instance;
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (result != VK_SUCCESS)
    {
        printf("Failed to create VkInstance%i\n", static_cast<int>(result));
        return -1;
    }
    
    // create a window surface using GLFW's helper function
    VkSurfaceKHR surface;
    result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS)
    {
        printf("Failed to create VkSurfaceKHR");
        return -1;
    }
    
    // get all available physical devices
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(count);
    vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());
    
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // note that these families may end up being the same family
    int32_t graphicsFamily = -1; // capable of rasterization graphics
    int32_t presentFamily = -1;  // capable of presenting to a surface

    for (auto pd : physicalDevices)
    {
        graphicsFamily = -1;
        presentFamily = -1;
        
        // A physical device can have multiple queue families that correspond to different/combined parts of the GPU.
        // Higher end NVIDIA GPUs for example often have a general graphics/compute/transfer family,
        // a dedicated compute family, and a dedicated transfer family.
        // Dedicated families may perform better and may run in parallel with other
        // families (e.g. a dedicated transfer family might operate directly through the gpu's memory controller)
        
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, queueFamilyProperties.data());
        
        for (size_t i = 0; i < count; i++)
        {
            if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
                graphicsFamily = i;
            
            bool presentationSupport = glfwGetPhysicalDevicePresentationSupport(instance, pd, i);
            
            uint32_t surfaceSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &surfaceSupport);
         
            if (presentationSupport && surfaceSupport)
                presentFamily = i;
        }
        
        if (graphicsFamily == -1 || presentFamily == -1)
            continue;
        
        // check for VK_KHR_swapchain support
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(count);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, availableExtensions.data());

        if (std::find_if(availableExtensions.begin(), availableExtensions.end(), [=](auto item) { return strcmp(item.extensionName, "VK_KHR_swapchain") == 0; } ) == availableExtensions.end())
            continue;

        // we've found a compatible device
        physicalDevice = pd;
        break;
    }
    
    std::vector<VkDeviceQueueCreateInfo> deviceQueues;
    
    // queues can have different priorities which may change the GPU resources they get,
    // in our case we'll just stick to a default 1.0
    float priority = 1.0f;
    
    deviceQueues.push_back(VkDeviceQueueCreateInfo {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,        // pNext
        0,              // flags (none)
        static_cast<uint32_t>(graphicsFamily), // we'll need at least a graphics queue
        1,              // create one queue
        &priority       // pass on priority (this must be an array if num queues is more than 1)
    });
    
    // only create a separate present queue if needed
    if (presentFamily != graphicsFamily)
    {
        deviceQueues.push_back(VkDeviceQueueCreateInfo {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            nullptr,        // pNext
            0,              // flags (none)
            static_cast<uint32_t>(presentFamily),
            1,              // create one queue
            &priority       // pass on priority (this must be an array if num queues is more than 1)
        });
    }
    
    // we'll need the swapchain extension (covered later)
    std::vector<const char*> deviceExtensions = { "VK_KHR_swapchain" };
    
#ifdef __APPLE__
    // not covered in the presentation: Apple implementations must add the VK_KHR_portability_subset extension
    // because MoltenVK doesn't cover 100% of the features in Vulkan 1.0
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif
    
    // Device creation takes our array of queues, and array of extensions
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = nullptr;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = deviceQueues.size();
    deviceInfo.pQueueCreateInfos = deviceQueues.data();
    deviceInfo.enabledLayerCount = 0; // device layers are deprecated, always pass 0 and nullptr
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.enabledExtensionCount = deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    
    VkDevice device;
    result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS)
    {
        printf("Failed to create VkDevice");
        return -1;
    }
    
    // the device created the queue as requested
    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    
    VkQueue presentQueue;
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
    
    // create a command pool
    // command pools are structures that allocate the memory necessary
    // to be able to record command buffers.
    VkCommandPoolCreateInfo commandPoolInfo {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    // command pools contain commands for a specific queue family
    // in our case we're using this commandbuffer to render graphics so we'll pass the graphics family
    commandPoolInfo.queueFamilyIndex = graphicsFamily;
    
    VkCommandPool commandPool;
    result = vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool);
    if (result != VK_SUCCESS)
    {
        printf("Failed to create VkCommandPool");
        return -1;
    }
    
    // allocate a command buffer from our command pool
    VkCommandBufferAllocateInfo cmdAllocInfo {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = nullptr;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // primary cmd buffers can be submitted to a queue directly
    cmdAllocInfo.commandBufferCount = 1; // we only need one command buffer in this sample
    cmdAllocInfo.commandPool = commandPool; // allocate from the command pool we just created
    
    // note that VkCommandPool is a pool! This means that when we destroy our VkCommandPool, our
    // allocated command buffers will automatically be destroyed as well.
    // we do have the option to destroy them manually if we wish through vkFreeCommandBuffers()
    VkCommandBuffer cmd;
    result = vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);
    if (result != VK_SUCCESS)
    {
        printf("Failed to allocate VkCommandBuffer");
        return -1;
    }
    
    // Get the surface capabilities to figure out the surface's
    // limits such as its min/max extent, image count, etc.
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    
    // clamp our selected window size to the min/max surface extent
    VkExtent2D selectedExtent;
    selectedExtent.width = std::clamp(800u, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
    selectedExtent.height = std::clamp(800u, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

    // clamp our desired image count (we'll pick 2 for now) and clamp between min/max image count
    uint32_t selectedImageCount = std::clamp(2u, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
    
    // we also need to check if we can use the surface's images as a color attachment
    // this is needed so we can draw to it, but if it isn't supported we could
    // draw to a different image and copy to the swapchain images instead
    if ((surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
        printf("Surface doesn't support IMAGE_USAGE_COLOR_ATTACHMENT_BIT");
        return -1;
    }
    // must support transfer dst for clearing the image
    if ((surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        printf("Surface doesn't support IMAGE_USAGE_TRANSFER_DST_BIT (required for clear image)");
        return -1;
    }
    
    // iterate the available surface formats and pick a format
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, surfaceFormats.data());
    
    VkSurfaceFormatKHR selectedFormat = surfaceFormats[0]; // fallback format
    for (const auto& format : surfaceFormats)
    {
        // ideally we find an sRGB format for better color accuracy
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            selectedFormat = format;
    }
    
    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext = nullptr;
    swapchainInfo.flags = 0;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = selectedImageCount;
    swapchainInfo.imageFormat = selectedFormat.format;
    swapchainInfo.imageColorSpace = selectedFormat.colorSpace;
    swapchainInfo.imageExtent = selectedExtent;
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
    
    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS)
    {
        printf("Failed to create VkSwapchainKHR");
        return -1;
    }
    
    // get the VkImages from our swapchain
    // these images are what we'll be rendering to
    vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
    std::vector<VkImage> swapchainImages(count);
    vkGetSwapchainImagesKHR(device, swapchain, &count, swapchainImages.data());
    
    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.flags = 0;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    // semaphores are for GPU-GPU synchronization
    // imageWaitSemaphore: Makes our command buffer wait on vkAcquireNextImageKHR to be finished
    // presentWaitSemaphore: Makes vkQueuePresentKHR wait on our commands to be done rendering
    VkSemaphore imageWaitSemaphore, presentWaitSemaphore;
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageWaitSemaphore);
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &presentWaitSemaphore);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Acquire the next image to render to
        // the frame might not immediately be ready (swapchain may stall for e.g. vsync)
        // so we must wait with either a semaphore (GPU-GPU sync) or a fence (CPU-GPU sync)
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapchain, UINT_MAX, imageWaitSemaphore, /* fence */ nullptr, &imageIndex);
        
        // describe how we'll start recording the command buffer
        // this is usually fairly simple for primary command buffers
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;
        vkBeginCommandBuffer(cmd, &beginInfo); // start recording
        
        // our image only has one mip level and one array layer
        VkImageSubresourceRange subresource{};
        subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource.baseMipLevel = 0;
        subresource.levelCount = 1;
        subresource.baseArrayLayer = 0;
        subresource.layerCount = 1;
        
        // use an image barrier to change the image's layout from UNDEFINED to TRANSFER_DST_OPTIMAL
        // this is necessary so that we can use the image in vkCmdClearColorImage()
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
        barrier.dstAccessMask = VK_ACCESS_NONE_KHR;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = graphicsFamily; // barriers can be used to transfer queue family "ownership" too
        barrier.dstQueueFamilyIndex = graphicsFamily; // this can be necessary in multi-queue renderers
        barrier.image = swapchainImages[imageIndex];
        barrier.subresourceRange = subresource;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // pick a clear color - float32 is in RGBA [0 - 1]
        VkClearColorValue clearColor {};
        clearColor.float32[0] = 1;
        clearColor.float32[1] = 0;
        clearColor.float32[2] = 1;
        clearColor.float32[3] = 1;
        vkCmdClearColorImage(cmd, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &subresource);
        
        // after we're done clearing/drawing to our image we need to prepare it for presentation
        // transition the image to PRESENT_SRC_KHR:
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        vkEndCommandBuffer(cmd); // end recording
        
        // this can be more optimal or specialized by picking a more specific pipeline stage
        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        
        // submit the command list to the graphics queue
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.pNext = nullptr;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageWaitSemaphore; // wait for the image to be ready before the commands can execute
        submit.pWaitDstStageMask = &waitStageMask;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &presentWaitSemaphore; // signal the present wait semaphore afterwards so present() can wait on it
        vkQueueSubmit(graphicsQueue, 1, &submit, nullptr);
        
        // after we're done rendering, we'll present our image to the screen.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &presentWaitSemaphore; // present after waiting is done
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = &result;
        vkQueuePresentKHR(presentQueue, &presentInfo);
        
        // wait for everything to be finished before we continue to the next frame
        // note: this is bad practice but the lecture is too short to teach other approaches
        vkDeviceWaitIdle(device);
    }
    
    // Clean up our created resources
    vkDestroySemaphore(device, imageWaitSemaphore, nullptr);
    vkDestroySemaphore(device, presentWaitSemaphore, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    
    glfwDestroyWindow(window);
    glfwTerminate();
}

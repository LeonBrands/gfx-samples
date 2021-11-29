#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <map>
#include <array>
#include <vector>
#include <fstream>

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
    
    // a swapchain swaps images between the presentation engine and the application
    // this way, we can work on rendering to one image, while the other is being read by a screen
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
    
    // semaphores are for GPU-GPU synchronization
    // imageWaitSemaphore: Makes our command buffer wait on vkAcquireNextImageKHR to be finished
    // presentWaitSemaphore: Makes vkQueuePresentKHR wait on our commands to be done rendering
    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.flags = 0;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkSemaphore imageWaitSemaphore, presentWaitSemaphore;
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageWaitSemaphore);
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &presentWaitSemaphore);
    
    // next we'll describe a render pass
    // renderpasses are like a pre-defined render graph
    // they define sub passes and how they interact with their (and each other's) attachments
    // this can help greatly improve performance on mobile devices
    // our renderpass will be fairly simple: 1 subpass with 1 color attachment
    
    // describe our color attachment:
    // - how its used
    // - how its loaded/stored
    // - what its layout will be before/after the pass
    VkAttachmentDescription colorAttachment {};
    colorAttachment.flags = 0;
    colorAttachment.format = selectedFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // msaa
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    // subpasses must describe their attachments and in what layout they wish to use them
    // during a renderpass, attachments are transitioned to a subpass's desired layout
    // thus our attachment starts as UNDEFINED, transitions to COLOR_ATTACHMENT during our subpass, and at the end of the renderpass it transitions to PRESENT_SRC
    VkAttachmentReference colorRef {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // describe a simple graphics (not compute) subpass with a single color attachment
    VkSubpassDescription subpass {};
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;
    
    // create a renderpass with the described color attachment and subpass
    VkRenderPassCreateInfo renderpassInfo {};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassInfo.pNext = nullptr;
    renderpassInfo.flags = 0;
    renderpassInfo.attachmentCount = 1;
    renderpassInfo.pAttachments = &colorAttachment;
    renderpassInfo.subpassCount = 1;
    renderpassInfo.pSubpasses = &subpass;
    renderpassInfo.dependencyCount = 0;
    renderpassInfo.pDependencies = nullptr;
    
    VkRenderPass renderpass;
    result = vkCreateRenderPass(device, &renderpassInfo, nullptr, &renderpass);
    if (result != VK_SUCCESS)
    {
        printf("Failed to create renderpass");
        return -1;
    }
    
    // We'll create an image view and a framebuffer for each swapchain image
    // Creating a framebuffer for the swapchain images is necessary to be able to render to them using our renderpass
    // the image view is required for framebuffer creation
    std::vector<VkImageView> swapchainImageViews(swapchainImages.size());
    std::vector<VkFramebuffer> framebuffers(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); i++)
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
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = selectedFormat.format;
        viewInfo.components = mapping;
        viewInfo.subresourceRange = swapchainSubresourceRange;
        
        result = vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]);
        if (result != VK_SUCCESS)
        {
            printf("Failed to create image view");
            return 1;
        }
        
        // a framebuffer is an image that can be used by a renderpass
        // the renderpass can write to this image or change its layout
        VkFramebufferCreateInfo framebufferInfo {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.pNext = nullptr;
        framebufferInfo.flags = 0;
        framebufferInfo.renderPass = renderpass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &swapchainImageViews[i];
        framebufferInfo.width = selectedExtent.width;
        framebufferInfo.height = selectedExtent.height;
        framebufferInfo.layers = 1;
        
        result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]);
        if (result != VK_SUCCESS)
        {
            printf("Failed to create framebuffer");
            return -1;
        }
    }
    
    // rendering your first triangle is a fair bit of work
    // the next couple hundred lines will work towards the creation of a "VkPipeline"
    // VkPipeline represents (in this case) the graphics pipeline
    // to minimize runtime cost, the majority of information has to be provided up front
    // this is different from OpenGL, where states are set to a default and you change them at will with gl...()
    
    // shaders are compiled from glsl to spirv using a compiler (e.g. glslc)
    // spirv is a binary format that we'll reeed in as a char (uint8_t) array
    std::ifstream vertexFile("../src/001_triangle/vertex.spv", std::ios::ate | std::ios::binary);
    size_t fileSize = (size_t) vertexFile.tellg();
    std::vector<char> vertexFileBuffer(fileSize);
    vertexFile.seekg(0);
    vertexFile.read(vertexFileBuffer.data(), fileSize);
    vertexFile.close();
    
    // pass the shader data on to the drivers through a "VkShaderModule"
    VkShaderModuleCreateInfo vertexShaderInfo {};
    vertexShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexShaderInfo.pNext = nullptr;
    vertexShaderInfo.flags = 0;
    vertexShaderInfo.codeSize = vertexFileBuffer.size();
    vertexShaderInfo.pCode = reinterpret_cast<uint32_t*>(vertexFileBuffer.data());
    
    VkShaderModule vertexShader;
    result = vkCreateShaderModule(device, &vertexShaderInfo, nullptr, &vertexShader);
    if (result != VK_SUCCESS)
    {
        printf("Failed to create vertex shader module");
        return -1;
    }
    
    // the same process is done for our fragment shader
    std::ifstream fragmentFile("../src/001_triangle/fragment.spv", std::ios::ate | std::ios::binary);
    fileSize = (size_t) fragmentFile.tellg();
    std::vector<char> fragmentFileBuffer(fileSize);
    fragmentFile.seekg(0);
    fragmentFile.read(fragmentFileBuffer.data(), fileSize);
    fragmentFile.close();
    
    VkShaderModuleCreateInfo fragmentShaderInfo {};
    fragmentShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragmentShaderInfo.pNext = nullptr;
    fragmentShaderInfo.flags = 0;
    fragmentShaderInfo.codeSize = fragmentFileBuffer.size();
    fragmentShaderInfo.pCode = reinterpret_cast<uint32_t*>(fragmentFileBuffer.data());
    
    VkShaderModule fragmentShader;
    result = vkCreateShaderModule(device, &fragmentShaderInfo, nullptr, &fragmentShader);
    
    // the pipeline layout describes how GPU resources (textures, buffers, etc) are bound to the shader
    // so that the shader can access it
    // our sample shaders have no bindings so this structure receives default values:
    VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext = nullptr;
    pipelineLayoutInfo.flags = 0;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    
    VkPipelineLayout pipelineLayout;
    result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    
    // describe our vertex and fragment shader (shader stage, entry point) for the pipeline
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        VkPipelineShaderStageCreateInfo {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertexShader,
            "main",
            nullptr
        },
        VkPipelineShaderStageCreateInfo {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragmentShader,
            "main",
            nullptr
        }
    };
    
    // the vertex input state is used to describe how the driver should interpret a vertex buffer if available
    // e.g. how much data each vertex contains and how its laid out
    // this sample doesn't use a vertex buffer so default values are passed:
    VkPipelineVertexInputStateCreateInfo pipelineVertexInput {};
    pipelineVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInput.pNext = nullptr;
    pipelineVertexInput.flags = 0;
    pipelineVertexInput.vertexBindingDescriptionCount = 0;
    pipelineVertexInput.pVertexBindingDescriptions = nullptr;
    pipelineVertexInput.vertexAttributeDescriptionCount = 0;
    pipelineVertexInput.pVertexAttributeDescriptions = nullptr;
    
    // the input assembly state describes what kind of topology is created in the draw call
    VkPipelineInputAssemblyStateCreateInfo pipelineAssemblyState {};
    pipelineAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineAssemblyState.pNext = nullptr;
    pipelineAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // we're drawing triangles
    pipelineAssemblyState.primitiveRestartEnable = false;
    
    // the tesselation state describes what happens during the optional tesselation stage of the pipeline
    // we have no special behaviour during this state so default values are passed:
    VkPipelineTessellationStateCreateInfo pipelineTesselationState {};
    pipelineTesselationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    pipelineTesselationState.pNext = nullptr;
    pipelineTesselationState.flags = 0;
    pipelineTesselationState.patchControlPoints = 0;
    
    // describe the viewport and scissor
    VkViewport viewport;
    viewport.width = selectedExtent.width;
    viewport.height = selectedExtent.height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    viewport.x = 0;
    viewport.y = 0;
    
    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = selectedExtent;
    
    VkPipelineViewportStateCreateInfo pipelineViewportState {};
    pipelineViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportState.pNext = nullptr;
    pipelineViewportState.flags = 0;
    pipelineViewportState.viewportCount = 1;
    pipelineViewportState.pViewports = &viewport;
    pipelineViewportState.scissorCount = 1;
    pipelineViewportState.pScissors = &scissor;
    
    // the rasterization state contains various properties that you may be used to setting dynamically in opengl
    // but these are instead described up-front, such as polygon culling, line widths and depth clamping
    VkPipelineRasterizationStateCreateInfo pipelineRasterizationState {};
    pipelineRasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationState.pNext = nullptr;
    pipelineRasterizationState.flags = 0;
    pipelineRasterizationState.depthClampEnable = false;
    pipelineRasterizationState.rasterizerDiscardEnable = false;
    pipelineRasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineRasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineRasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    pipelineRasterizationState.depthBiasEnable = false;
    pipelineRasterizationState.depthBiasConstantFactor = 0;
    pipelineRasterizationState.depthBiasClamp = 0;
    pipelineRasterizationState.depthBiasSlopeFactor = 0;
    pipelineRasterizationState.lineWidth = 1;
    
    // describe how/if the pipeline should apply MSAA
    // these default values simply disable it:
    VkPipelineMultisampleStateCreateInfo pipelineMultiSampleState {};
    pipelineMultiSampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultiSampleState.pNext = nullptr;
    pipelineMultiSampleState.flags = 0;
    pipelineMultiSampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineMultiSampleState.sampleShadingEnable = false;
    pipelineMultiSampleState.minSampleShading = 1;
    pipelineMultiSampleState.pSampleMask = nullptr;
    pipelineMultiSampleState.alphaToOneEnable = false;
    pipelineMultiSampleState.alphaToCoverageEnable = false;
    
    // describe how fragments calculated by the rasterizer interact with an optional depth and stencil buffer
    // these default values disable depth and stencil testing:
    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState {};
    pipelineDepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilState.pNext = nullptr;
    pipelineDepthStencilState.flags = 0;
    pipelineDepthStencilState.depthTestEnable = false;
    pipelineDepthStencilState.depthWriteEnable = false;
    pipelineDepthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    pipelineDepthStencilState.depthBoundsTestEnable = false;
    pipelineDepthStencilState.stencilTestEnable = false;
    pipelineDepthStencilState.front = {};
    pipelineDepthStencilState.back = {};
    pipelineDepthStencilState.minDepthBounds = 0;
    pipelineDepthStencilState.maxDepthBounds = 1;
    
    // describe if and how fragments are blended at the end of the pipeline
    // these default values disable blending:
    VkPipelineColorBlendAttachmentState colorBlendAttachment {};
    colorBlendAttachment.blendEnable = false;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
    
    VkPipelineColorBlendStateCreateInfo pipelineColorBlendState {};
    pipelineColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendState.pNext = nullptr;
    pipelineColorBlendState.flags = 0;
    pipelineColorBlendState.logicOpEnable = false;
    pipelineColorBlendState.logicOp = VK_LOGIC_OP_NO_OP;
    pipelineColorBlendState.attachmentCount = 1;
    pipelineColorBlendState.pAttachments = &colorBlendAttachment;
    pipelineColorBlendState.blendConstants[0] = 0;
    pipelineColorBlendState.blendConstants[1] = 0;
    pipelineColorBlendState.blendConstants[2] = 0;
    pipelineColorBlendState.blendConstants[3] = 0;

    // dynamic states can help prevent having to recreate pipelines for
    // values that could change a lot (e.g. a viewport size or scissor)
    // if a dynamic state is enabled, it must also be set during render time (e.g. vkCmdSetViewport() for VK_DYNAMIC_STATE_VIEWPORT)
    VkPipelineDynamicStateCreateInfo pipelineDynamicState {};
    pipelineDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicState.pNext = nullptr;
    pipelineDynamicState.flags = 0;
    pipelineDynamicState.dynamicStateCount = 0;
    pipelineDynamicState.pDynamicStates = nullptr;
    
    // gather all the information we've previously described to make up the final pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderpass;
    pipelineInfo.subpass = 0; // subpass index 0
    pipelineInfo.basePipelineHandle = nullptr;
    pipelineInfo.basePipelineIndex = 0;
    
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    
    pipelineInfo.pVertexInputState = &pipelineVertexInput;
    pipelineInfo.pInputAssemblyState = &pipelineAssemblyState;
    pipelineInfo.pTessellationState = &pipelineTesselationState;
    
    pipelineInfo.pViewportState = &pipelineViewportState;
    pipelineInfo.pRasterizationState = &pipelineRasterizationState;
    pipelineInfo.pMultisampleState = &pipelineMultiSampleState;
    pipelineInfo.pDepthStencilState = &pipelineDepthStencilState;
    pipelineInfo.pColorBlendState = &pipelineColorBlendState;
    pipelineInfo.pDynamicState = &pipelineDynamicState;
    
    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline);
    
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
        
        // pick a clear color - float32 is in RGBA [0 - 1]
        VkClearValue clearValue {};
        clearValue.color.float32[0] = 0;
        clearValue.color.float32[1] = 0;
        clearValue.color.float32[2] = 0;
        clearValue.color.float32[3] = 1;
        
        VkRenderPassBeginInfo renderpassBegin {};
        renderpassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpassBegin.pNext = nullptr;
        renderpassBegin.renderPass = renderpass;
        renderpassBegin.framebuffer = framebuffers[imageIndex];
        renderpassBegin.renderArea = VkRect2D { VkOffset2D { 0, 0 }, selectedExtent };
        renderpassBegin.clearValueCount = 1;
        renderpassBegin.pClearValues = &clearValue;
        
        vkCmdBeginRenderPass(cmd, &renderpassBegin, VK_SUBPASS_CONTENTS_INLINE);
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0); // draw 3 vertices, one instance, first vertex = 0, first instance = 0
        vkCmdEndRenderPass(cmd);
        
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
    
    // all resources created with vkCreate... have to be vkDestroy...ed
    // we'll do so here at the end of the application
    // note that these resources may still be in use by the application
    // so it is recommended to call vkDeviceWaitIdle(device) prior to destroying them.
    vkDeviceWaitIdle(device);
    
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);
    
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        vkDestroyFramebuffer(device, framebuffers[i], nullptr);
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
    
    vkDestroyRenderPass(device, renderpass, nullptr);
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

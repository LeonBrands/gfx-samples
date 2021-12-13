// this sample takes 002_vertex_buffer and restructures it with helper functions/classes
// to make it easier to add more features.
// no new concepts are introduced, the majority of this project's source code is the same as the
// previous sample, other than some moving around/refactoring.

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <map>
#include <array>
#include <vector>
#include <fstream>

#include "utils/preprocessor.hpp"
#include "utils/extensions.hpp"
#include "utils/layers.hpp"
#include "utils/physical_device.hpp"
#include "utils/swapchain.hpp"
#include "utils/shader.hpp"
#include "utils/memory.hpp"
#include "utils/buffer.hpp"

VkInstance createInstance();
VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window);
VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, int32_t graphicsFamily, int32_t presentFamily);
VkCommandPool createCommandPool(VkDevice device, uint32_t graphicsFamily);
VkCommandBuffer allocateCommandBuffer(VkDevice device, VkCommandPool cmdPool);
VkRenderPass createRenderpass(VkDevice device, VkFormat format);
VkPipelineLayout createPipelineLayout(VkDevice device);
VkPipeline createPipeline(VkDevice device, Swapchain& swap, VkRenderPass renderpass, VkPipelineLayout pipelineLayout, VkShaderModule vertexShader, VkShaderModule fragmentShader);

int main() {
    // default GLFW window creation except we disable OpenGL context creation
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 800, "003_restructure", nullptr, nullptr);
    
    VkInstance instance = createInstance();
    VkSurfaceKHR surface = createSurface(instance, window);
    
    QueueFamilies families;
    VkPhysicalDevice physicalDevice = PhysicalDevice::select(instance, surface, &families);
    
    VkDevice device = createDevice(instance, physicalDevice, families.graphics, families.present);
    VkQueue graphicsQueue; vkGetDeviceQueue(device, families.graphics, 0, &graphicsQueue);
    VkQueue presentQueue; vkGetDeviceQueue(device, families.present, 0, &presentQueue);
    
    VkCommandPool commandPool = createCommandPool(device, families.graphics);
    VkCommandBuffer cmd = allocateCommandBuffer(device, commandPool);
    
    Swapchain swap = Swapchain::create(device, physicalDevice, surface, families.graphics, families.present);
    
    VkRenderPass renderpass = createRenderpass(device, swap.format);
    auto swapchainImages = swap.getImages(device);
    auto swapchainImageViews = swap.getImageViews(device);
    auto swapchainFramebuffers = swap.getFramebuffers(device, renderpass);
    
    // semaphores are for GPU-GPU synchronization
    // imageWaitSemaphore: Makes our command buffer wait on vkAcquireNextImageKHR to be finished
    // presentWaitSemaphore: Makes vkQueuePresentKHR wait on our commands to be done rendering
    VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
    VkSemaphore imageWaitSemaphore, presentWaitSemaphore;
    THROW_IF_FAILED(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageWaitSemaphore));
    THROW_IF_FAILED(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &presentWaitSemaphore));
  
    VkShaderModule vertexShader = Shader::load(device, "../003_restructure/vertex.spv");
    VkShaderModule fragmentShader = Shader::load(device, "../003_restructure/fragment.spv");
    
    VkPipelineLayout pipelineLayout = createPipelineLayout(device);
    VkPipeline pipeline = createPipeline(device, swap, renderpass, pipelineLayout, vertexShader, fragmentShader);
    
    // vertex: { float3 pos, float3 color }
    // 0.0 is the center of the screen, -1,-1 is top left and 1,1 is bottom right
    // colors are simple 0-1 RGB values
    std::vector<float> vertices {
        //  vertex              color
        -0.5, -0.5, 0.0,     1.0, 0.0, 0.0,
        0.5, 0.5, 0.0,       0.0, 1.0, 0.0,
        -0.5, 0.5, 0.0,      0.0, 0.0, 1.0,
        
        -0.5, -0.5, 0.0,     1.0, 0.0, 0.0,
        0.5, -0.5, 0.0,      0.0, 0.0, 1.0,
        0.5, 0.5, 0.0,       0.0, 1.0, 0.0
    };
    
    std::unique_ptr<Buffer> vertexBuffer = Buffer::createUploadBuffer(device, physicalDevice, families, sizeof(float) * vertices.size(), vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Acquire the next image to render to
        // the frame might not immediately be ready (swapchain may stall for e.g. vsync)
        // so we must wait with either a semaphore (GPU-GPU sync) or a fence (CPU-GPU sync)
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swap.swapchain, UINT_MAX, imageWaitSemaphore, /* fence */ nullptr, &imageIndex);
        
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
        renderpassBegin.framebuffer = swapchainFramebuffers[imageIndex];
        renderpassBegin.renderArea = VkRect2D { VkOffset2D { 0, 0 }, swap.extent };
        renderpassBegin.clearValueCount = 1;
        renderpassBegin.pClearValues = &clearValue;
        
        vkCmdBeginRenderPass(cmd, &renderpassBegin, VK_SUBPASS_CONTENTS_INLINE);
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer->buffer, &offset);
        vkCmdDraw(cmd, vertices.size(), 1, 0, 0);
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
        VkResult result;
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &presentWaitSemaphore; // present after waiting is done
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swap.swapchain;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = &result;
        vkQueuePresentKHR(presentQueue, &presentInfo);
        
        // wait for everything to be finished before we continue to the next frame
        // note: this is bad practice but it allows us to focus on the rest of Vulkan first
        vkDeviceWaitIdle(device);
    }
    
    // all resources created with vkCreate... have to be vkDestroy...ed
    // we'll do so here at the end of the application
    // note that these resources may still be in use by the application
    // so it is recommended to call vkDeviceWaitIdle(device) prior to destroying them.
    vkDeviceWaitIdle(device);
    
    // even though unique ptrs automatically destroy,
    // this still has to happen before destruction of VkDevice
    // so we'll do so manually here
    vertexBuffer.reset();
    
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);
    
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
    
    vkDestroyRenderPass(device, renderpass, nullptr);
    vkDestroySemaphore(device, imageWaitSemaphore, nullptr);
    vkDestroySemaphore(device, presentWaitSemaphore, nullptr);
    vkDestroySwapchainKHR(device, swap.swapchain, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    
    glfwDestroyWindow(window);
    glfwTerminate();
}

VkInstance createInstance()
{
    Extensions extensionHelper{};
    extensionHelper.addRequiredGLFW();
    extensionHelper.add("VK_KHR_get_physical_device_properties2"); // always add if available -> required on MoltenVK
    auto extensions = extensionHelper.get();
    auto layers = Layers::get();
    
    // VkApplicationInfo is largely informative and usually just gives drivers additional information
    // for debugging purposes.
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "003_restructure";
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
    THROW_IF_FAILED(vkCreateInstance(&instanceInfo, nullptr, &instance));
    return instance;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window)
{
    // create a window surface using GLFW's helper function
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create VkSurfaceKHR from GLFW window");
    
    return surface;
}

VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, int32_t graphicsFamily, int32_t presentFamily)
{
    std::vector<VkDeviceQueueCreateInfo> deviceQueues;
    
    // queues can have different priorities which may change the GPU resources they get,
    // in our case we'll just stick to a default 1.0
    std::array<float, 2> priorities = { 1, 1 };
    
    deviceQueues.push_back(VkDeviceQueueCreateInfo {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,        // pNext
        0,              // flags (none)
        static_cast<uint32_t>(graphicsFamily), // we'll need at least a graphics queue
        1,              // create one queue
        priorities.data()       // pass on priority (this must be an array if num queues is more than 1)
    });
    
    // only create a separate present queue if needed
    if (graphicsFamily != presentFamily)
    {
        deviceQueues.push_back(VkDeviceQueueCreateInfo {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            nullptr,        // pNext
            0,              // flags (none)
            static_cast<uint32_t>(presentFamily),
            1,              // create one queue
            priorities.data()       // pass on priority (this must be an array if num queues is more than 1)
        });
    }

    Extensions ext { physicalDevice };
    ext.add("VK_KHR_swapchain", true);
    ext.add("VK_KHR_portability_subset");
    auto extensions = ext.get();
    
    // Device creation takes our array of queues, and array of extensions
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = nullptr;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = deviceQueues.size();
    deviceInfo.pQueueCreateInfos = deviceQueues.data();
    deviceInfo.enabledLayerCount = 0; // device layers are deprecated, always pass 0 and nullptr
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.enabledExtensionCount = extensions.size();
    deviceInfo.ppEnabledExtensionNames = extensions.data();
    
    VkDevice device;
    THROW_IF_FAILED(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));
    
    return device;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t graphicsFamily)
{
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
    THROW_IF_FAILED(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));
    
    return commandPool;
}

VkCommandBuffer allocateCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
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
    THROW_IF_FAILED(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd));
    
    return cmd;
}

VkRenderPass createRenderpass(VkDevice device, VkFormat format)
{
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
    colorAttachment.format = format;
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
    THROW_IF_FAILED(vkCreateRenderPass(device, &renderpassInfo, nullptr, &renderpass));
    
    return renderpass;
}

VkPipelineLayout createPipelineLayout(VkDevice device)
{
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
    THROW_IF_FAILED(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
    
    return pipelineLayout;
}

VkPipeline createPipeline(VkDevice device, Swapchain& swap, VkRenderPass renderpass, VkPipelineLayout pipelineLayout, VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    // Pipeline could certainly use a more intricate abstraction that allows deeper configuration of its parameters
    // this sample just stuffs everything away in a function however
    
    // rendering your first triangle is a fair bit of work
    // the next bit of creation code will work towards the creation of a "VkPipeline"
    // VkPipeline represents (in this case) the graphics pipeline
    // to minimize runtime cost, the majority of information has to be provided up front
    // this is different from OpenGL, where states are set to a default and you change them at will with gl...()
    
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
    
    // describe in what kind of chunks the vertex buffer is split up
    VkVertexInputBindingDescription vertexBinding {};
    vertexBinding.stride = sizeof(float) * (3 + 3); // 6 floats (float3 pos, float3 color)
    vertexBinding.binding = 0;
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // used on a per vertex basis
    
    // describe how the vertex binding above maps to vertex input in the shader
    std::array<VkVertexInputAttributeDescription, 2> vertexAttributes {
        VkVertexInputAttributeDescription { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        VkVertexInputAttributeDescription { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3 } // offset by 3 floats because of pos
    };
    
    // the vertex input state is used to describe how the driver should interpret our vertex buffer
    VkPipelineVertexInputStateCreateInfo pipelineVertexInput {};
    pipelineVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInput.pNext = nullptr;
    pipelineVertexInput.flags = 0;
    pipelineVertexInput.vertexBindingDescriptionCount = 1;
    pipelineVertexInput.pVertexBindingDescriptions = &vertexBinding;
    pipelineVertexInput.vertexAttributeDescriptionCount = vertexAttributes.size();
    pipelineVertexInput.pVertexAttributeDescriptions = vertexAttributes.data();
    
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
    viewport.width = swap.extent.width;
    viewport.height = swap.extent.height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    viewport.x = 0;
    viewport.y = 0;
    
    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = swap.extent;
    
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
    THROW_IF_FAILED(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline));
    
    return pipeline;
}

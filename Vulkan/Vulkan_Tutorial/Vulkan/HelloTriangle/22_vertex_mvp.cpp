//
//  HelloTriangleApplication.cpp
//  Vulkan
//
//  Created by luxiaodong on 2022/5/14.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <optional>
#include <cstdint>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const int WIDTH = 400;
const int HEIGHT = 300;
const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_portability_subset",
};

struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;
    
    static std::vector<VkVertexInputBindingDescription> getBindingDescription()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions = {};
        VkVertexInputBindingDescription binding = {};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(binding);
        return bindingDescriptions;
    }
    
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescription()
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {};
        VkVertexInputAttributeDescription attr1 = {};
        attr1.binding = 0;
        attr1.location = 0;
        attr1.offset = offsetof(Vertex, pos);
        attr1.format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions.push_back(attr1);
        
        VkVertexInputAttributeDescription attr2 = {};
        attr2.binding = 0;
        attr2.location = 1;
        attr2.offset = offsetof(Vertex, color);
        attr2.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions.push_back(attr2);
        return attributeDescriptions;
    }
};

struct UniformBufferObject
{
    glm::mat4 modelMat;
    glm::mat4 viewMat;
    glm::mat4 projMat;
};

const std::vector<Vertex> vertices =
{
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {0,1,2,2,3,0};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    
    bool isComplete(){
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    
private:
    GLFWwindow* m_window;
    VkInstance m_instance;
    VkSurfaceKHR m_surfaceKHR;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    QueueFamilyIndices m_indices;
    VkSwapchainKHR m_swapchainKHR;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkSurfaceFormatKHR m_surfaceFormatKHR;
    VkExtent2D m_swapchainExtent;
    VkPipeline m_graphicsPipeline;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkFence> m_inFlightFences;
    int m_currentFrame = 0;
    
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;

private:
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createVertexBuffer();
        createVertexIndexBuffer();
        createUniformBuffer();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        recordCommandBuffers();
        createSemaphores();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            drawFrame();
        }
        
        vkDeviceWaitIdle(m_device);
    }
    
    void cleanup()
    {
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }
        
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
        vkFreeMemory(m_device, m_uniformMemory, nullptr);
        
        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        vkFreeMemory(m_device, m_indexMemory, nullptr);
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexMemory, nullptr);
        
        vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        
        for(const auto& frameBuffer : m_framebuffers)
        {
            vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
        }
        
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

        for(const auto& imageView : m_swapchainImageViews)
        {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
        
        vkDestroySwapchainKHR(m_device, m_swapchainKHR, nullptr);
        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surfaceKHR, nullptr);
        vkDestroyInstance(m_instance, nullptr);
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
    
    void createInstance()
    {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        uint32_t propertiesCount = 0;
        vkEnumerateInstanceExtensionProperties("", &propertiesCount, nullptr);
        std::vector<VkExtensionProperties> extensionProperties(propertiesCount);
        vkEnumerateInstanceExtensionProperties("", &propertiesCount, extensionProperties.data());

        std::vector<const char *> extensions;
        std::cout << "InstanceExtensionProperties : " << propertiesCount << std::endl;
        for(auto& property : extensionProperties)
        {
            std::cout << "\t" << property.extensionName << std::endl;
            extensions.push_back(property.extensionName);
        }
        
        //add all extension
        createInfo.enabledExtensionCount = propertiesCount;
        createInfo.ppEnabledExtensionNames = extensions.data();
        if(vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        {
            std::runtime_error("failed to create instance!");
        }
    }
    
    void createSurface()
    {
        if(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surfaceKHR) != VK_SUCCESS)
        {
            std::runtime_error("failed to create window surface!");
        }
    }
    
    void pickPhysicalDevice()
    {
        uint32_t physicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());
        std::cout << "physicalDeviceCount : " << physicalDeviceCount << std::endl;
        m_physicalDevice = physicalDevices[0];
        
        printPhysicalDeviceFeatures();
        printPhysicalDeviceProperties();
        printPhysicalDeviceMemoryProperties();
    }
    
    void printPhysicalDeviceFeatures()
    {
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(m_physicalDevice, &features);
        std::cout << "physicalDeviceFeatures" << std::endl;
        std::cout << "\t" << "depthClamp " << features.depthClamp << std::endl;
        std::cout << "\t" << "geometryShader " << features.geometryShader << std::endl;
        std::cout << "\t" << "multiViewport " << features.multiViewport << std::endl;
    }
    
    void printPhysicalDeviceProperties()
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
        std::cout << "physicalDeviceProperties" << std::endl;
        std::cout << "\t" << "deviceName " << properties.deviceName << std::endl;
        std::cout << "\t" << "apiVersion " << properties.apiVersion << std::endl;
        std::cout << "\t" << "driverVersion " << properties.driverVersion << std::endl;
        std::cout << "\t" << "vendorID " << properties.vendorID << std::endl;
        std::cout << "\t" << "deviceID " << properties.deviceID << std::endl;
        std::cout << "\t" << "deviceType " << properties.deviceType << std::endl;
        std::cout << "\t" << "deviceLimits" << std::endl;
        std::cout << "\t\t" << "maxImageDimension2D " << properties.limits.maxImageDimension2D << std::endl;
        std::cout << "\t\t" << "maxImageArrayLayers " << properties.limits.maxImageArrayLayers << std::endl;
        std::cout << "\t\t" << "maxTexelBufferElements " << properties.limits.maxTexelBufferElements << std::endl;
        std::cout << "\t\t" << "maxUniformBufferRange " << properties.limits.maxUniformBufferRange << std::endl;
        std::cout << "\t\t" << "maxStorageBufferRange " << properties.limits.maxStorageBufferRange << std::endl;
        std::cout << "\t\t" << "maxPushConstantsSize " << properties.limits.maxPushConstantsSize << std::endl;
        std::cout << "\t\t" << "maxMemoryAllocationCount " << properties.limits.maxMemoryAllocationCount << std::endl;
        std::cout << "\t\t" << "maxSamplerAllocationCount " << properties.limits.maxSamplerAllocationCount << std::endl;
        std::cout << "\t\t" << "maxBoundDescriptorSets " << properties.limits.maxBoundDescriptorSets << std::endl;
        std::cout << "\t\t" << "maxPerStageDescriptorSamplers " << properties.limits.maxPerStageDescriptorSamplers << std::endl;
        std::cout << "\t\t" << "maxPerStageDescriptorUniformBuffers " << properties.limits.maxPerStageDescriptorStorageBuffers << std::endl;
        std::cout << "\t\t" << "maxPerStageDescriptorSampledImages " << properties.limits.maxPerStageDescriptorSampledImages << std::endl;
        std::cout << "\t\t" << "maxPerStageDescriptorStorageImages " << properties.limits.maxPerStageDescriptorStorageImages << std::endl;
        std::cout << "\t\t" << "maxPerStageDescriptorInputAttachments " << properties.limits.maxPerStageDescriptorInputAttachments << std::endl;
        std::cout << "\t\t" << "maxPerStageResources " << properties.limits.maxPerStageResources << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetSamplers " << properties.limits.maxDescriptorSetSamplers << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetUniformBuffers " << properties.limits.maxDescriptorSetUniformBuffers << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetUniformBuffersDynamic " << properties.limits.maxDescriptorSetUniformBuffersDynamic << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetStorageBuffers " << properties.limits.maxDescriptorSetStorageBuffers << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetStorageBuffersDynamic " << properties.limits.maxDescriptorSetStorageBuffersDynamic << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetSampledImages " << properties.limits.maxDescriptorSetSampledImages << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetStorageImages " << properties.limits.maxDescriptorSetStorageImages << std::endl;
        std::cout << "\t\t" << "maxDescriptorSetInputAttachments " << properties.limits.maxDescriptorSetInputAttachments << std::endl;
        std::cout << "\t\t" << "maxVertexInputAttributes " << properties.limits.maxVertexInputAttributes << std::endl;
        std::cout << "\t\t" << "maxVertexInputBindings " << properties.limits.maxVertexInputBindings << std::endl;
        std::cout << "\t\t" << "maxVertexInputAttributeOffset " << properties.limits.maxVertexInputAttributeOffset << std::endl;
        std::cout << "\t\t" << "maxVertexInputBindingStride " << properties.limits.maxVertexInputBindingStride << std::endl;
        std::cout << "\t\t" << "maxVertexOutputComponents " << properties.limits.maxVertexOutputComponents << std::endl;
        std::cout << "\t\t" << "maxFragmentInputComponents " << properties.limits.maxFragmentInputComponents << std::endl;
        std::cout << "\t\t" << "maxFragmentOutputAttachments " << properties.limits.maxFragmentOutputAttachments << std::endl;
        std::cout << "\t\t" << "maxFragmentDualSrcAttachments " << properties.limits.maxFragmentDualSrcAttachments << std::endl;
        std::cout << "\t\t" << "maxFragmentCombinedOutputResources " << properties.limits.maxFragmentCombinedOutputResources << std::endl;
        std::cout << "\t\t" << "maxComputeSharedMemorySize " << properties.limits.maxComputeSharedMemorySize << std::endl;
        std::cout << "\t\t" << "maxDrawIndexedIndexValue " << properties.limits.maxDrawIndexedIndexValue << std::endl;
        std::cout << "\t\t" << "maxDrawIndirectCount " << properties.limits.maxDrawIndirectCount << std::endl;
        std::cout << "\t\t" << "maxViewports " << properties.limits.maxViewports << std::endl;
        std::cout << "\t\t" << "maxFramebufferWidth " << properties.limits.maxFramebufferWidth << std::endl;
        std::cout << "\t\t" << "maxVertexInputAttributes " << properties.limits.maxVertexInputAttributes << std::endl;
        std::cout << "\t\t" << "maxFramebufferHeight " << properties.limits.maxFramebufferHeight << std::endl;
        std::cout << "\t\t" << "maxFramebufferLayers " << properties.limits.maxFramebufferLayers << std::endl;
        std::cout << "\t\t" << "maxColorAttachments " << properties.limits.maxColorAttachments << std::endl;
        std::cout << "\t\t" << "maxClipDistances " << properties.limits.maxClipDistances << std::endl;
        std::cout << "\t\t" << "maxCullDistances " << properties.limits.maxCullDistances << std::endl;
        std::cout << "\t\t" << "maxCombinedClipAndCullDistances " << properties.limits.maxCombinedClipAndCullDistances << std::endl;
        std::cout << "\t\t" << "discreteQueuePriorities " << properties.limits.discreteQueuePriorities << std::endl;
    }
    
    void printPhysicalDeviceMemoryProperties()
    {
        VkPhysicalDeviceMemoryProperties properties;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &properties);
        std::cout << "physicalDeviceMemoryProperties" << std::endl;
        std::cout << "\t" << "memoryTypeCount " << properties.memoryTypeCount << std::endl;
        std::cout << "\t" << "memoryHeapCount " << properties.memoryHeapCount << std::endl;
    }
    
    void queueFamilyProperty()
    {
        uint32_t queueFamilyPropertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
        
        uint32_t i = 0;
        for(auto properties : queueFamilyProperties)
        {
            if( properties.queueFlags & VK_QUEUE_GRAPHICS_BIT )
            {
                m_indices.graphicsFamily = i;
            }

            VkBool32 supported;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surfaceKHR, &supported);
            if(supported)
            {
                m_indices.presentFamily = i;
            }
            
            if(m_indices.graphicsFamily.has_value() && m_indices.presentFamily.has_value())
            {
                break;
            }
            
            i++;
        }
    }
    
    void createLogicalDevice()
    {
        queueFamilyProperty();
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        uint32_t graphicsFamily = m_indices.graphicsFamily.value();
        uint32_t presentFamily = m_indices.presentFamily.value();
        std::vector<uint32_t> familyIndexs = {};
        
        if ( graphicsFamily == presentFamily )
        {
            familyIndexs.push_back(graphicsFamily);
        }
        else
        {
            familyIndexs.push_back(graphicsFamily);
            familyIndexs.push_back(presentFamily);
        }
        
        for(uint32_t index : familyIndexs)
        {
            VkDeviceQueueCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.queueCount = 1;
            createInfo.queueFamilyIndex = index;
            float queuePriority = 1.0f;
            createInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(createInfo);
        }
        
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        
        if( vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create device!");
        }
        
        vkGetDeviceQueue(m_device, graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, presentFamily, 0, &m_presentQueue);
    }
    
    void createSwapchain()
    {
        VkSurfaceCapabilitiesKHR surfaceCapabilityKHR;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surfaceKHR, &surfaceCapabilityKHR);
        uint32_t minImageCount = surfaceCapabilityKHR.minImageCount + 1;
        if(minImageCount > surfaceCapabilityKHR.maxImageCount)
        {
            minImageCount = surfaceCapabilityKHR.maxImageCount;
        }

        VkSurfaceFormatKHR surfaceFormatKHR = chooseSurfaceFormat();
        VkPresentModeKHR presentModeKHR = choosePresentMode();
        
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surfaceKHR;
        createInfo.minImageCount = minImageCount;
        createInfo.imageFormat = surfaceFormatKHR.format;
        createInfo.imageColorSpace = surfaceFormatKHR.colorSpace;
        createInfo.imageExtent = surfaceCapabilityKHR.currentExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        
        uint32_t graphicsFamily = m_indices.graphicsFamily.value();
        uint32_t presentFamily = m_indices.presentFamily.value();
        
        if(graphicsFamily == presentFamily)
        {
            createInfo.imageSharingMode =  VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        else
        {
            std::vector<uint32_t> familyIndexs = {graphicsFamily, presentFamily};
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(familyIndexs.size());
            createInfo.pQueueFamilyIndices = familyIndexs.data();
        }

        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentModeKHR;
        createInfo.clipped = VK_TRUE; //???????????????
        createInfo.oldSwapchain = nullptr;

        if(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchainKHR) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create swapchainKHR");
        }
        
        uint32_t swapchainImageCount = 0;
        vkGetSwapchainImagesKHR(m_device, m_swapchainKHR, &swapchainImageCount, nullptr);
        m_swapchainImages.resize(swapchainImageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapchainKHR, &swapchainImageCount, m_swapchainImages.data());
        m_surfaceFormatKHR = surfaceFormatKHR;
        m_swapchainExtent = surfaceCapabilityKHR.currentExtent;
    }
    
    void createImageViews()
    {
        m_swapchainImageViews.resize(m_swapchainImages.size());
        for(size_t i = 0; i < m_swapchainImages.size(); ++i)
        {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.flags = 0;
            createInfo.format = m_surfaceFormatKHR.format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if( vkCreateImageView(m_device, &createInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS )
            {
                throw std::runtime_error("failed to create imageView!");
            }
        }
    }
    
    void createGraphicsPipeline()
    {
//        VkShaderModule vertModule = createShaderModule("vert.spv");
//        VkShaderModule vertModule = createShaderModule("input_vert.spv");
        VkShaderModule vertModule = createShaderModule("layout_vert.spv");
        VkShaderModule fragModule = createShaderModule("frag.spv");

        VkPipelineShaderStageCreateInfo vertShader = {};
        vertShader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShader.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShader.module = vertModule;
        vertShader.pName = "main";

        VkPipelineShaderStageCreateInfo fragShader = {};
        fragShader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShader.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShader.module = fragModule;
        fragShader.pName = "main";
        
        std::vector<VkVertexInputBindingDescription> bindingDescriptions = Vertex::getBindingDescription();
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions = Vertex::getAttributeDescription();

        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.flags = 0;
        vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInput.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.flags = 0;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        
        VkViewport vp;
        vp.x = 0;
        vp.y = 0;
        vp.width = m_swapchainExtent.width;
        vp.height = m_swapchainExtent.height;
        vp.minDepth = 0;
        vp.maxDepth = 1;
        
        VkRect2D scissor;
        scissor.offset = {0, 0};
        scissor.extent = m_swapchainExtent;

        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.flags = 0;
        viewport.viewportCount = 1;
        viewport.pViewports = &vp;
        viewport.scissorCount = 1;
        viewport.pScissors = &scissor;
        
        VkPipelineRasterizationStateCreateInfo rasterization = {};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.flags = 0;
        rasterization.depthClampEnable = VK_FALSE;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.depthBiasEnable = VK_FALSE;
        rasterization.depthBiasConstantFactor = 0.0f;
        rasterization.depthBiasClamp = VK_FALSE;
        rasterization.depthBiasSlopeFactor = 0.0f;
        rasterization.lineWidth = 1.0f;
        
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.flags = 0;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample.sampleShadingEnable = VK_FALSE;
        multisample.pSampleMask = nullptr;
        multisample.alphaToCoverageEnable = VK_FALSE;
        multisample.alphaToOneEnable = VK_FALSE;
        
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.flags = 0;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.minDepthBounds = 1.0f;
        
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        
        VkPipelineColorBlendStateCreateInfo colorBlend = {};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.flags = 0;
        colorBlend.logicOpEnable = VK_FALSE;
        colorBlend.logicOp = VK_LOGIC_OP_COPY;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &colorBlendAttachment;
        
        VkPipelineShaderStageCreateInfo stages[] = {vertShader, fragShader};
        createPipelineLayout();
        
        VkGraphicsPipelineCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.stageCount = 2;
        createInfo.pStages = stages;
        createInfo.pVertexInputState = &vertexInput;
        createInfo.pInputAssemblyState = &inputAssembly;
        createInfo.pTessellationState = nullptr;
        createInfo.pViewportState = &viewport;
        createInfo.pRasterizationState = &rasterization;
        createInfo.pMultisampleState = &multisample;
        createInfo.pDepthStencilState = &depthStencil;
        createInfo.pColorBlendState = &colorBlend;
        createInfo.pDynamicState = nullptr;
        createInfo.layout = m_pipelineLayout;
        createInfo.renderPass = m_renderPass;
        createInfo.subpass = 0;
        createInfo.basePipelineHandle = VK_NULL_HANDLE;
        createInfo.basePipelineIndex = 0;

        if( vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }
    
    void createRenderPass()
    {
        // xxx = {}  === memery set 0
        VkAttachmentDescription attachmentDescription;
        attachmentDescription.flags = 0;
        attachmentDescription.format = m_surfaceFormatKHR.format;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference attachmentReference;
        attachmentReference.attachment = 0;
        attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescription subpassDescription;
        subpassDescription.flags = 0;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &attachmentReference;
        subpassDescription.pResolveAttachments = nullptr;
        subpassDescription.pDepthStencilAttachment = nullptr;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;

        VkRenderPassCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &attachmentDescription;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassDescription;
        createInfo.dependencyCount = 0;
        createInfo.pDependencies = nullptr;

        if( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create renderpass");
        }
    }
    
    void createFramebuffers()
    {
        m_framebuffers.resize(m_swapchainImageViews.size());
        
        for(size_t i = 0; i < m_swapchainImageViews.size(); ++i)
        {
            VkFramebufferCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = m_renderPass;
            createInfo.attachmentCount = 1;
            createInfo.pAttachments = &m_swapchainImageViews[i];
            createInfo.width = m_swapchainExtent.width;
            createInfo.height = m_swapchainExtent.height;
            createInfo.layers = 1;
            
            if( vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS )
            {
                throw std::runtime_error("failed to create frame buffer!");
            }
        }
    }
    
    void createCommandPool()
    {
        VkCommandPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.queueFamilyIndex = m_indices.graphicsFamily.value();
        
        if(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }
    
    void createCommandBuffers()
    {
        m_commandBuffers.resize(m_framebuffers.size());
        
        VkCommandBufferAllocateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        createInfo.commandPool = m_commandPool;
        createInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        createInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
        
        if( vkAllocateCommandBuffers(m_device, &createInfo, m_commandBuffers.data()) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create command buffer!");
        }
    }
    
    void createVertexBuffer()
    {
        VkDeviceSize size = sizeof(Vertex) * vertices.size();
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        void* data; //????????????????????????????????????????????????.????????????????????????????????????.
        vkMapMemory(m_device, stagingMemory, 0, size, 0, &data);
        memcpy(data, vertices.data(), size); //?????????copy????????????. ??????????????????copy, ?????? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        vkUnmapMemory(m_device, stagingMemory);

        // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ?????????vkMapMemory
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_vertexBuffer, m_vertexMemory);
        
        copyBuffer(stagingBuffer, m_vertexBuffer, size);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    }
    
    void createVertexIndexBuffer()
    {
        VkDeviceSize size = sizeof(indices[0])*indices.size();
        createBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_indexBuffer, m_indexMemory);
        
        void* data;
        vkMapMemory(m_device, m_indexMemory, 0, size, 0, &data);
        memcpy(data, indices.data(), size);
        vkUnmapMemory(m_device, m_indexMemory);
    }
    
    void createUniformBuffer()
    {
        VkDeviceSize size = sizeof(UniformBufferObject);
        createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_uniformBuffer, m_uniformMemory);
    }
    
    void createDescriptorPool()
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1; //????????? uniform buffer.
        
        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.maxSets = 1;
        createInfo.poolSizeCount = 1;
        createInfo.pPoolSizes = &poolSize;
        
        if( vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create descriptorPool!");
        }
    }
    
    void createDescriptorSets()
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;
        
        //?????????????????????
        if( vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to allocate descriptorSets");
        }
        //???????????????buffer
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = m_uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        //???????????????????????????
        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_descriptorSet;
        writeSet.dstBinding = 0;
        writeSet.dstArrayElement = 0;
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeSet.descriptorCount = 1;
        writeSet.pBufferInfo = &bufferInfo;
        writeSet.pImageInfo = nullptr;
        writeSet.pTexelBufferView = nullptr;
        //?????????????????????
        vkUpdateDescriptorSets(m_device, 1, &writeSet, 0, nullptr);
    }

    void recordCommandBuffers()
    {
        int i = 0;
        for(const auto& commandBuffer : m_commandBuffers)
        {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            if( vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS )
            {
                throw std::runtime_error("failed to begin command buffer!");
            }

            VkClearValue clearColor = {0.0f,0.0f,0.0f,0.0f};
            VkRenderPassBeginInfo passBeginInfo = {};
            passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            passBeginInfo.renderPass = m_renderPass;
            passBeginInfo.framebuffer = m_framebuffers[i];
            passBeginInfo.renderArea.offset = {0, 0};
            passBeginInfo.renderArea.extent = m_swapchainExtent;
            passBeginInfo.clearValueCount = 1;
            passBeginInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
//            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
//            vkCmdDraw(commandBuffer, static_cast<uint_fast32_t>(vertices.size()), 1, 0, 0);
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint_fast32_t>(indices.size()), 1, 0, 0, 0);
            vkCmdEndRenderPass(commandBuffer);

            if( vkEndCommandBuffer(commandBuffer) != VK_SUCCESS )
            {
                throw std::runtime_error("failed to end command buffer!");
            }
            
            i++;
        }
    }
    
    void createSemaphores()
    {
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        
        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.flags = 0;
        
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //????????????????????????.??????????????????????????????
        
        for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if( (vkCreateSemaphore(m_device, &createInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) |
                (vkCreateSemaphore(m_device, &createInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS) |
                (vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]) ))
            {
                throw std::runtime_error("failed to create semaphorses!");
            }
        }
    }
    
    void drawFrame()
    {
        //fence???????????????????????????????????????
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
        
        uint32_t imageIndex = 0;
        vkAcquireNextImageKHR(m_device, m_swapchainKHR, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

        updateUniformBuffer();
        
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

        //??????????????????????????????????????????fence
        if(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to queue submit!");
        }
        
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapchainKHR;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;
        
        if(vkQueuePresentKHR(m_presentQueue, &presentInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to queue present!");
        }
        
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    
    // ===================== helper function =====================
    
    void updateUniformBuffer()
    {
        static int i = 0;
        i++;
        UniformBufferObject ubo{};
        ubo.modelMat = glm::rotate(glm::mat4(1.0f), glm::radians(1.0f*i), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.viewMat = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.projMat = glm::perspective(glm::radians(45.0f), m_swapchainExtent.width / (float) m_swapchainExtent.height, 0.1f, 10.0f);
        ubo.projMat[1][1] *= -1;

        void* data;
        vkMapMemory(m_device, m_uniformMemory, 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(m_device, m_uniformMemory);
    }
    
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, VkBuffer &buffer, VkDeviceMemory& memory)
    {
        VkBufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.size = size;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &m_indices.graphicsFamily.value();
        if(vkCreateBuffer(m_device, &createInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create buffer!");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size; // > createInfo.size, need alignment
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, flags);
    
        if( vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate memory!");
        }

        //buffer?????????memory????????????, memory??????????????????.
        vkBindBufferMemory(m_device, buffer, memory, 0);
    }
    
    //?????????copy?????????.
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        createInfo.commandPool = m_commandPool;
        createInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        createInfo.commandBufferCount = 1;
        
        VkCommandBuffer commandBuffer;
        if( vkAllocateCommandBuffers(m_device, &createInfo, &commandBuffer) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create command buffer!");
        }
        
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        VkBufferCopy bufferCopy = {};
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopy);
        
        vkEndCommandBuffer(commandBuffer);
    
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        
        vkQueueWaitIdle(m_graphicsQueue);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    }
    
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
        
        for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            if( (typeFilter & (1<<i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) )
            {
                return i;
            }
        }
        
        throw std::runtime_error("failed to find suitable memory type!");
    }
    
    VkSurfaceFormatKHR chooseSurfaceFormat()
    {
        uint32_t surfaceFormatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surfaceKHR, &surfaceFormatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> surfaceFormatsKHR(surfaceFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surfaceKHR, &surfaceFormatCount, surfaceFormatsKHR.data());
        
        for(const auto &surfaceFormat : surfaceFormatsKHR)
        {
            if(surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
               surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return surfaceFormat;
            }
        }
        
        return surfaceFormatsKHR[0];
    }
    
    VkPresentModeKHR choosePresentMode()
    {
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surfaceKHR, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModesKHR(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surfaceKHR, &presentModeCount, presentModesKHR.data());
        
        for(const auto &presentMode : presentModesKHR)
        {
            if(presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return presentMode;
            }
        }
        
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    
    VkShaderModule createShaderModule(const std::string& filename)
    {
        std::vector<char> code = readFile(filename);
        
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        
        VkShaderModule shaderModule;
        if( vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }
    
    void createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorCount = 1; //??????????????????????????????
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.bindingCount = 1;
        createInfo.pBindings = &binding;

        if( vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create descriptorSetLayout!");
        }
    }
    
    void createPipelineLayout()
    {
        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.setLayoutCount = 1;
        createInfo.pSetLayouts = &m_descriptorSetLayout;
        createInfo.pushConstantRangeCount = 0;
        createInfo.pPushConstantRanges = nullptr;

        if( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create layout!");
        }
    }
    
    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
           throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }
    
};

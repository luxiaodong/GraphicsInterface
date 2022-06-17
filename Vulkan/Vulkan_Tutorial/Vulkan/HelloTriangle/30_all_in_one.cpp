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
#include <array>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

const int WIDTH = 400;
const int HEIGHT = 300;

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
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    
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
        attr1.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions.push_back(attr1);
        
        VkVertexInputAttributeDescription attr2 = {};
        attr2.binding = 0;
        attr2.location = 1;
        attr2.offset = offsetof(Vertex, color);
        attr2.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions.push_back(attr2);
        
        VkVertexInputAttributeDescription attr3 = {};
        attr3.binding = 0;
        attr3.location = 2;
        attr3.offset = offsetof(Vertex, texCoord);
        attr3.format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions.push_back(attr3);
        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

struct UniformBufferObject
{
    glm::mat4 modelMat;
    glm::mat4 viewMat;
    glm::mat4 projMat;
};

struct Texture2D
{
    uint32_t width;
    uint32_t height;
    void* data;
    VkDeviceSize size;
    VkFormat format;
};

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
    QueueFamilyIndices m_familyIndices;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    
    VkSwapchainKHR m_swapchainKHR;
    VkExtent2D m_swapchainExtent;
    VkSurfaceFormatKHR m_surfaceFormatKHR;
    uint32_t m_swapchainImageCount = 2;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    
    VkPipeline m_graphicsPipeline;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;
    
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    std::vector<VkSemaphore> m_renderFinishedSemaphores; //渲染完成
    std::vector<VkSemaphore> m_imageAvailableSemaphores; //送往交换链完成
    std::vector<VkFence> m_inFlightFences;
    int m_currentFrame = 0;
    
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_vertexIndices;
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorSet m_descriptorSet;
    VkDescriptorPool m_descriptorPool;

    VkImage m_textureImage;
    VkDeviceMemory m_textureMemory;
    VkImageView m_textureImageView;
    VkSampler m_textureSampler;
    Texture2D m_textrue2D;
    
    VkImage m_depthImage;
    VkDeviceMemory m_depthMemory;
    VkImageView m_depthImageView;
    
    uint32_t m_mipLevels;
    
    VkImage m_msaaImage;
    VkDeviceMemory m_msaaMemory;
    VkImageView m_msaaImageView;
    VkSampleCountFlagBits m_msaaFlag = VK_SAMPLE_COUNT_1_BIT;

private:
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicDevice();
        
        createSwapchain();
        createSwapchainImageView();
        
        createCommandPool();
        loadModel();
        createVertexBuffer();
        createVertexIndexBuffer();
        
        loadTexture();
        createTextureBuffer();
        createTextureSampler();
        
        createMsaaBuffer();
        
        createDepthBuffer();
        
        createUniformBuffer();
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();
        
        createRenderPass();
        createPipelineLayout();
        createGraphicsPipeline();
        createFramebuffers();
        
        createCommandBuffersAndRecord();
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
        vkDestroyImageView(m_device, m_msaaImageView, nullptr);
        vkDestroyImage(m_device, m_msaaImage, nullptr);
        vkFreeMemory(m_device, m_msaaMemory, nullptr);
        vkDestroyImageView(m_device, m_depthImageView, nullptr);
        vkDestroyImage(m_device, m_depthImage, nullptr);
        vkFreeMemory(m_device, m_depthMemory, nullptr);
        vkDestroySampler(m_device, m_textureSampler, nullptr);
        vkDestroyImageView(m_device, m_textureImageView, nullptr);
        vkDestroyImage(m_device, m_textureImage, nullptr);
        vkFreeMemory(m_device, m_textureMemory, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
        vkFreeMemory(m_device, m_uniformMemory, nullptr);
        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
        vkFreeMemory(m_device, m_indexMemory, nullptr);
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexMemory, nullptr);

        for(size_t i = 0; i < m_swapchainImageCount; ++i)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }
        
        for(const auto& imageView : m_swapchainImageViews)
        {
            vkDestroyImageView(m_device, imageView, nullptr);
        }
        
        vkFreeCommandBuffers(m_device, m_commandPool, m_swapchainImageCount, m_commandBuffers.data());
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        
        for(const auto& frameBuffer : m_framebuffers)
        {
            vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
        }
        
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
                
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
        if( glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surfaceKHR) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create windowSurface!");
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
        m_msaaFlag = getMaxUsableSampleCount();
    }
    
    void createLogicDevice()
    {
        m_familyIndices = findQueueFamilyIndices();
        uint32_t graphicsFamily = m_familyIndices.graphicsFamily.value();
        uint32_t presentFamily = m_familyIndices.presentFamily.value();
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
        
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
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
        createInfo.flags = 0;
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        
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
        
        m_surfaceFormatKHR.format = VK_FORMAT_B8G8R8A8_UNORM;
        m_surfaceFormatKHR.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        m_swapchainExtent.width = WIDTH * 2;
        m_swapchainExtent.height = HEIGHT * 2;
        
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.flags = 0;
        createInfo.surface = m_surfaceKHR;
        createInfo.minImageCount = m_swapchainImageCount;
        createInfo.imageFormat = m_surfaceFormatKHR.format;
        createInfo.imageColorSpace = m_surfaceFormatKHR.colorSpace;
        createInfo.imageExtent = m_swapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        
        if( vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchainKHR) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create swapchain!");
        }
    }
    
    void createSwapchainImageView()
    {
        uint32_t swapchainImageCount = 0;
        vkGetSwapchainImagesKHR(m_device, m_swapchainKHR, &swapchainImageCount, nullptr);
        m_swapchainImages.resize(swapchainImageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapchainKHR, &swapchainImageCount, m_swapchainImages.data());
        m_swapchainImageCount = swapchainImageCount;
        
        m_swapchainImageViews.resize(m_swapchainImageCount);
        for(uint32_t i = 0; i < m_swapchainImageCount; i++)
        {
            m_swapchainImageViews[i] = createImageView(m_swapchainImages[i], m_surfaceFormatKHR.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }
    }
    
    void createCommandPool()
    {
        VkCommandPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.queueFamilyIndex = m_familyIndices.graphicsFamily.value();
        
        if(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }
    
    void loadModel()
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "viking.obj")) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                    m_vertices.push_back(vertex);
                }

                m_vertexIndices.push_back(uniqueVertices[vertex]);
            }
        }
    }
    
    void createVertexBuffer()
    {
        VkDeviceSize size = sizeof(Vertex) * m_vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        createBufferAndMemoryThenBind(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        void* data; //给这个应用程序访问这块内存的指针.从物理地址映射到虚拟地址.
        vkMapMemory(m_device, stagingMemory, 0, size, 0, &data);
        memcpy(data, m_vertices.data(), size); //将数据copy到内存里. 可能不是立即copy, 详见 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        vkUnmapMemory(m_device, stagingMemory);

        // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 不能用vkMapMemory
        createBufferAndMemoryThenBind(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_vertexBuffer, m_vertexMemory);

        copyBuffer(stagingBuffer, m_vertexBuffer, size);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    }
    
    void createVertexIndexBuffer()
    {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        
        VkDeviceSize size = sizeof(m_vertexIndices[0]) * m_vertexIndices.size();
        createBufferAndMemoryThenBind(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);
        
        void* data;
        vkMapMemory(m_device, stagingMemory, 0, size, 0, &data);
        memcpy(data, m_vertexIndices.data(), size);
        vkUnmapMemory(m_device, stagingMemory);
        
        createBufferAndMemoryThenBind(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexMemory);
        
        copyBuffer(stagingBuffer, m_indexBuffer, size);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
    }
    
    void loadTexture()
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("viking.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }
        
        m_textrue2D.width = texWidth;
        m_textrue2D.height = texHeight;
        m_textrue2D.data = pixels;
        m_textrue2D.format = VK_FORMAT_R8G8B8A8_SRGB;
        m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight))));
    }
    
    void createTextureBuffer()
    {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        
        VkDeviceSize size = m_textrue2D.width * m_textrue2D.height * 4;
        createBufferAndMemoryThenBind(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);
        
        void* data;
        vkMapMemory(m_device, stagingMemory, 0, size, 0, &data);
        memcpy(data, m_textrue2D.data, size);
        vkUnmapMemory(m_device, stagingMemory);
        
        stbi_image_free(m_textrue2D.data);
        
        createImageAndMemoryThenBind(m_textrue2D.format, m_textrue2D.width, m_textrue2D.height, m_mipLevels, VK_SAMPLE_COUNT_1_BIT,
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                      VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                      m_textureImage, m_textureMemory);
    
        m_textureImageView = createImageView(m_textureImage, m_textrue2D.format, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels);

        transitionImageLayout(m_textureImage, m_textrue2D.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_mipLevels);
        copyBufferToImage(stagingBuffer, m_textureImage, m_textrue2D.width, m_textrue2D.height);
        // transitionImageLayout(m_textureImage, m_textrue2D.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        generateMipmaps(m_textureImage, m_textrue2D.format, m_textrue2D.width, m_textrue2D.height, m_mipLevels);
        
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
    }
    
    void createTextureSampler()
    {
        VkSamplerCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.magFilter = VK_FILTER_LINEAR;
        createInfo.minFilter = VK_FILTER_LINEAR;
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.mipLodBias = 0.0f;
        createInfo.anisotropyEnable = VK_FALSE;
        createInfo.maxAnisotropy = 1.0f; //各项异性支持的最大值
        createInfo.compareEnable = VK_FALSE;
        createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        createInfo.minLod = 0;
        createInfo.maxLod = m_mipLevels;
        createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; //寻址模式是 VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER 时设置
        createInfo.unnormalizedCoordinates = VK_FALSE; // false,(0,1), true,(0,width/height)
        
        if( vkCreateSampler(m_device, &createInfo, nullptr, &m_textureSampler) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create sampler!");
        }
    }
    
    void createMsaaBuffer()
    {
        createImageAndMemoryThenBind(m_surfaceFormatKHR.format, m_swapchainExtent.width, m_swapchainExtent.height, 1, m_msaaFlag,
                                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                     VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_msaaImage, m_msaaMemory);
        m_msaaImageView = createImageView(m_msaaImage, m_surfaceFormatKHR.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
    
    void createDepthBuffer()
    {
        createImageAndMemoryThenBind(VK_FORMAT_D32_SFLOAT, m_swapchainExtent.width, m_swapchainExtent.height, 1,
                                     m_msaaFlag, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                     VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     m_depthImage, m_depthMemory);
        m_depthImageView = createImageView(m_depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
        transitionImageLayout(m_depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
    }

    void createUniformBuffer()
    {
        VkDeviceSize size = sizeof(UniformBufferObject);
        createBufferAndMemoryThenBind(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    m_uniformBuffer, m_uniformMemory);
    }
    
    void createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding vertexBinding = {};
        vertexBinding.binding = 0;
        vertexBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vertexBinding.descriptorCount = 1;
        vertexBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        vertexBinding.pImmutableSamplers = nullptr;
        
        VkDescriptorSetLayoutBinding textureBinding = {};
        textureBinding.binding = 1;
        textureBinding.descriptorCount = 1;
        textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding binding[] = {vertexBinding, textureBinding};
        
        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.bindingCount = 2;
        createInfo.pBindings = binding;

        if ( vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create descriptorSetLayout!");
        }
    }
    
    void createDescriptorPool()
    {
        VkDescriptorPoolSize poolSize1 = {};
        poolSize1.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize1.descriptorCount = 1;
        
        VkDescriptorPoolSize poolSize2 = {};
        poolSize2.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize2.descriptorCount = 1;
        
        VkDescriptorPoolSize poolSize[] = {poolSize1, poolSize2};

        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.poolSizeCount = 2;
        createInfo.pPoolSizes = poolSize;
        createInfo.maxSets = 1;
        
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
        
        if( vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptorSets!");
        }
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageView = m_textureImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.sampler = m_textureSampler;
        
        VkWriteDescriptorSet writeSetBuffer = {};
        writeSetBuffer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSetBuffer.dstSet = m_descriptorSet;
        writeSetBuffer.dstBinding = 0;
        writeSetBuffer.dstArrayElement = 0;
        writeSetBuffer.descriptorCount = 1;
        writeSetBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeSetBuffer.pImageInfo = nullptr;
        writeSetBuffer.pBufferInfo = &bufferInfo;
        writeSetBuffer.pTexelBufferView = nullptr;
        
        VkWriteDescriptorSet writeSetImage = {};
        writeSetImage.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSetImage.dstSet = m_descriptorSet;
        writeSetImage.dstBinding = 1;
        writeSetImage.dstArrayElement = 0;
        writeSetImage.descriptorCount = 1;
        writeSetImage.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeSetImage.pImageInfo = &imageInfo;
        writeSetImage.pBufferInfo = nullptr;
        writeSetImage.pTexelBufferView = nullptr;
        
        VkWriteDescriptorSet writeSets[] = {writeSetBuffer, writeSetImage};
        vkUpdateDescriptorSets(m_device, 2, writeSets, 0, nullptr);
    }
    
    void createRenderPass()
    {
        VkAttachmentDescription msaaAttachmentDescription = {};
        msaaAttachmentDescription.flags = 0;
        msaaAttachmentDescription.format = m_surfaceFormatKHR.format;
        msaaAttachmentDescription.samples = m_msaaFlag;
        msaaAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        msaaAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        msaaAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        msaaAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        msaaAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkAttachmentDescription depthAttachmentDescription = {};
        depthAttachmentDescription.flags = 0;
        depthAttachmentDescription.format = VK_FORMAT_D32_SFLOAT;
        depthAttachmentDescription.samples = m_msaaFlag;
        depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        VkAttachmentDescription colorAttachmentDescription = {};
        colorAttachmentDescription.flags = 0;
        colorAttachmentDescription.format = m_surfaceFormatKHR.format;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference msaaAttachmentReference = {};
        msaaAttachmentReference.attachment = 0;
        msaaAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference depthAttachmentReference = {};
        depthAttachmentReference.attachment = 1;
        depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference colorAttachmentReference = {};
        colorAttachmentReference.attachment = 2;
        colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescription subpassDescription = {};
        subpassDescription.flags = 0;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &msaaAttachmentReference;
        subpassDescription.pResolveAttachments = &colorAttachmentReference;
        subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;
        
        VkAttachmentDescription attachmentDescription[] = {msaaAttachmentDescription, depthAttachmentDescription, colorAttachmentDescription};
        
        VkRenderPassCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.attachmentCount = 3;
        createInfo.pAttachments = attachmentDescription;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassDescription;
        createInfo.dependencyCount = 0;
        createInfo.pDependencies = nullptr;

        if( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create renderpass!");
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
        
        if( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipelineLayout!");
        }
    }
    
    void createGraphicsPipeline()
    {
        VkShaderModule vertModule = createShaderModule("depth_vert.spv");
        VkShaderModule fragModule = createShaderModule("depth_frag.spv");
        
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
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.depthBiasEnable = VK_FALSE;
        rasterization.depthBiasConstantFactor = 0.0f;
        rasterization.depthBiasClamp = VK_FALSE;
        rasterization.depthBiasSlopeFactor = 0.0f;
        rasterization.lineWidth = 1.0f;
        
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.flags = 0;
        multisample.rasterizationSamples = m_msaaFlag;
        multisample.sampleShadingEnable = VK_FALSE;
        multisample.pSampleMask = nullptr;
        multisample.alphaToCoverageEnable = VK_FALSE;
        multisample.alphaToOneEnable = VK_FALSE;
        
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.flags = 0;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
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
    
    void createFramebuffers()
    {
        m_framebuffers.resize(m_swapchainImageCount);
        
        for(uint32_t i = 0; i < m_swapchainImageCount; ++i)
        {
            VkImageView attachments[] = {m_msaaImageView, m_depthImageView, m_swapchainImageViews[i]};
            
            VkFramebufferCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.flags = 0;
            createInfo.renderPass = m_renderPass;
            createInfo.attachmentCount = 3;
            createInfo.pAttachments = attachments;
            createInfo.width = m_swapchainExtent.width;
            createInfo.height = m_swapchainExtent.height;
            createInfo.layers = 1;
            
            if( vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS )
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }
    
    void createCommandBuffersAndRecord()
    {
        m_commandBuffers.resize(m_swapchainImageCount);
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = m_swapchainImageCount;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        
        if( vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to allocate commandBuffers!");
        }
        
        for(uint32_t i = 0; i < m_swapchainImageCount; ++i)
        {
            recordCommandBuffer(i);
        }
    }
    
    void createSemaphores()
    {
        m_renderFinishedSemaphores.resize(m_swapchainImageCount);
        m_imageAvailableSemaphores.resize(m_swapchainImageCount);
        m_inFlightFences.resize(m_swapchainImageCount);
        
        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.flags = 0;
        
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //创建时设置为发出.发出了下面才能接受到

        for(uint32_t i = 0; i < m_swapchainImageCount; ++i)
        {
            if( vkCreateSemaphore(m_device, &createInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_device, &createInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
               vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create semaphore and fence!");
            }
        }
    }
    
    void drawFrame()
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
        
        uint32_t imageIndex = 0;
        vkAcquireNextImageKHR(m_device, m_swapchainKHR, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

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
        
        updateUniformBuffer();
        
        if(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed submit to graphics queue");
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
        
        m_currentFrame = (m_currentFrame + 1) % m_swapchainImageCount;
    }
    
    // ----------------- helper function ---------------

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
    
    VkSampleCountFlagBits getMaxUsableSampleCount()
    {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }
    
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
    {
        // 检查格式是否支持线性插值
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, imageFormat, &formatProperties);
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        {
            throw std::runtime_error("texture image format does not support linear blitting!");
        }
        
        VkCommandBuffer commandBuffer = beginSingleCommands();
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        endSingleCommands(commandBuffer);
    }
    
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t lodLevel)
    {
        VkCommandBuffer commandBuffer = beginSingleCommands();
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = lodLevel;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        
        if( newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if( hasStencilComponent(format) )
            {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }
        else
        {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(commandBuffer,sourceStage, destinationStage,0,0, nullptr,0, nullptr,1, &barrier);
        endSingleCommands(commandBuffer);
    }
    
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        VkCommandBuffer commandBuffer = beginSingleCommands();
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = { width, height, 1};

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        endSingleCommands(commandBuffer);
    }
    
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleCommands();
        
        VkBufferCopy bufferCopy = {};
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopy);
        
        endSingleCommands(commandBuffer);
    }
    
    void createImageAndMemoryThenBind(VkFormat format, uint32_t width, uint32_t height, uint32_t lodLevels, VkSampleCountFlagBits sampleFlag, VkImageUsageFlags usage, VkImageTiling tiling, VkMemoryPropertyFlags flags, VkImage &image, VkDeviceMemory &imageMemory)
    {
        VkImageCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.format = format;
        createInfo.extent.width = width;
        createInfo.extent.height = height;
        createInfo.extent.depth = 1.0f;
        createInfo.mipLevels = lodLevels;
        createInfo.arrayLayers = 1;
        createInfo.samples = sampleFlag;
        createInfo.tiling = tiling;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
        if( vkCreateImage(m_device, &createInfo, nullptr, &image) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create iamge!");
        }
                
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, flags);
    
        if( vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate memory!");
        }
        
        vkBindImageMemory(m_device, image, imageMemory, 0);
    }
    
    void createBufferAndMemoryThenBind(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, VkBuffer &buffer, VkDeviceMemory& memory)
    {
        VkBufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.size = size;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &m_familyIndices.graphicsFamily.value();
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

        //buffer相当于memory的头信息, memory是真正的内存.
        vkBindBufferMemory(m_device, buffer, memory, 0);
    }
    
    VkCommandBuffer beginSingleCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }
    
    void endSingleCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
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
    
    void recordCommandBuffer(int index)
    {
        VkCommandBuffer commandBuffer = m_commandBuffers[index];
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        
        if( vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to begin command buffer!");
        }
        
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        
        VkRenderPassBeginInfo passBeginInfo = {};
        passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passBeginInfo.renderPass = m_renderPass;
        passBeginInfo.framebuffer = m_framebuffers[index];
        passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        passBeginInfo.pClearValues = clearValues.data();
        passBeginInfo.renderArea.offset = {0, 0};
        passBeginInfo.renderArea.extent = m_swapchainExtent;
        
        vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // inline 是什么
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
        //vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_vertexIndices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
        
        if( vkEndCommandBuffer(commandBuffer) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to end command buffer!");
        }
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
    
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t LodLevel)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspectFlags;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = LodLevel;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if( vkCreateImageView(m_device, &createInfo, nullptr, &imageView) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create imageView!");
        }
        return imageView;
    }
    
    bool hasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
    
    QueueFamilyIndices findQueueFamilyIndices()
    {
        QueueFamilyIndices indices;
        uint32_t queueFamilyPropertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
        
        uint32_t i = 0;
        for(auto properties : queueFamilyProperties)
        {
            if( properties.queueFlags & VK_QUEUE_GRAPHICS_BIT )
            {
                indices.graphicsFamily = i;
            }

            VkBool32 supported;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surfaceKHR, &supported);
            if(supported)
            {
                indices.presentFamily = i;
            }
            
            if(indices.graphicsFamily.has_value() && indices.presentFamily.has_value())
            {
                break;
            }
            
            i++;
        }
        
        return indices;
    }
};

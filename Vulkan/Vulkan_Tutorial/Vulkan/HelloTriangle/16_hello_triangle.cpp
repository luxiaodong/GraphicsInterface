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
    QueueFamilyIndices m_indices;

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
//        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSemaphores();
    }
    
    void mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            drawFrame();
        }
    }
    
    void cleanup()
    {
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
    }
    
    void createSwapchain()
    {
    }
    
    void createImageViews()
    {
    }
    
    void createRenderPass()
    {
    }

    void createGraphicsPipeline()
    {
    }
    
    void createFramebuffers()
    {
    }
    
    void createCommandPool()
    {
    }
    
    void createCommandBuffers()
    {
    }
    
    void createSemaphores()
    {
     
    }
    
    void drawFrame()
    {
     
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

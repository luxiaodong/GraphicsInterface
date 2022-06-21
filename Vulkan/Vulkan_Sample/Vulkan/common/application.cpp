
#include "application.h"

const std::vector<const char*> validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_portability_subset",
};

Application::Application(std::string title) : m_title(title)
{
}

Application::~Application()
{
}

void Application::run()
{
    init();
    loop();
    clear();
}

void Application::init()
{
    createWindow();
    createInstance();
    createSurface();
    choosePhysicalDevice();
    createLogicDeivce();
    createSwapchain();
    createSwapchainImageView();
    
    createPipelineCache();
    createCommandPool();
}

void Application::loop()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        render();
    }
    
    vkDeviceWaitIdle(m_device);
}

void Application::render()
{
    
}

void Application::clear()
{
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
    
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

void Application::resize(int width, int height)
{
}

void Application::createWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
}

void Application::createInstance()
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

void Application::createSurface()
{
    if( glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surfaceKHR) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create windowSurface!");
    }
}

void Application::choosePhysicalDevice()
{
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());
    std::cout << "physicalDeviceCount : " << physicalDeviceCount << std::endl;
    m_physicalDevice = physicalDevices[0];
}

void Application::createLogicDeivce()
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

void Application::createSwapchain()
{
    VkSurfaceCapabilitiesKHR surfaceCapabilityKHR;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surfaceKHR, &surfaceCapabilityKHR);
    
    m_surfaceFormatKHR.format = VK_FORMAT_B8G8R8A8_UNORM;
    m_surfaceFormatKHR.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    m_swapchainExtent.width = m_width * 2;
    m_swapchainExtent.height = m_height * 2;
    
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

void Application::createSwapchainImageView()
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

void Application::createPipelineCache()
{
    VkPipelineCacheCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if(vkCreatePipelineCache(m_device, &createInfo, nullptr, &m_pipelineCache) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipelineCache");
    }
}

void Application::createCommandPool()
{
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = m_familyIndices.graphicsFamily.value();
    
    if(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}


// -- helper function --

VkImageView Application::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t LodLevel)
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

VkShaderModule Application::createShaderModule(const std::string& filename)
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

std::vector<char> Application::readFile(const std::string& filename)
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

QueueFamilyIndices Application::findQueueFamilyIndices()
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

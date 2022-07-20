
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

static void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->keyboard(key, scancode, action, mods);
}

static void mouseCallback(GLFWwindow* window, double x, double y)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->mouse(x, y);
}

Application::Application(std::string title) : m_title(title)
{
#define PICCOLO_STR(s) #s
#define PICCOLO_XSTR(s) PICCOLO_STR(s)

    char const* vk_layer_path    = PICCOLO_XSTR(VK_LAYER_PATH);
    char const* vk_icd_filenames = PICCOLO_XSTR(VK_ICD_FILENAMES);
    setenv("VK_LAYER_PATH", vk_layer_path, 1);
    setenv("VK_ICD_FILENAMES", vk_icd_filenames, 1);
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
    initCamera();

    createWindow();
    createInstance();
    createSurface();
    choosePhysicalDevice();
    Tools::m_physicalDevice = m_physicalDevice;
    setEnabledFeatures();
    
    createLogicDeivce();
    Tools::m_device = m_device;
    Tools::m_deviceEnabledFeatures = m_deviceEnabledFeatures;
    Tools::m_deviceProperties = m_deviceProperties;
    createCommandPool();
    Tools::m_commandPool = m_commandPool;
    
    createSwapchain();
    createSwapchainImageView();
    
    createDepthBuffer();
    createOtherBuffer();
    createPipelineCache();
    
    createAttachmentDescription();
    createRenderPass();
    createFramebuffers();
    createCommandBuffers();

    createSemaphores();
//    initUi();
}

void Application::initCamera()
{}

void Application::setEnabledFeatures()
{}

void Application::loop()
{
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyboardCallback);
    glfwSetCursorPosCallback(m_window, mouseCallback);
    
    m_lastTimestamp = std::chrono::steady_clock::now();
    
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        
        logic();
        render();
        
        std::chrono::steady_clock::time_point tNow = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration_cast<std::chrono::duration<float>>(tNow - m_lastTimestamp).count();
        m_camera.update(deltaTime);
        
        m_averageDuration = m_averageDuration * 0.99 + deltaTime * 0.01;
        m_averageFPS = static_cast<int>(1.f/deltaTime);
        m_lastTimestamp = tNow;
//        std::cout << deltaTime << std::endl;
        
        vkDeviceWaitIdle(m_device);
    }
}

void Application::logic()
{
    if(m_pUi)
    {
        m_pUi->updateUI(m_averageFPS);
    }
}

void Application::updateRenderData()
{}

void Application::render()
{
    updateRenderData();
    
    if(m_pUi)
    {
        m_pUi->updateRenderData();
    }
    
    //fence需要手动重置为未发出的信号
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    
    uint32_t imageIndex = 0;
    vkAcquireNextImageKHR(m_device, m_swapchainKHR, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    VkCommandBuffer commandBuffer = m_commandBuffers[imageIndex];
    beginRenderCommandAndPass(commandBuffer, imageIndex);
    recordRenderCommand(commandBuffer);
    
    if(m_pUi)
    {
        m_pUi->recordRenderCommand(commandBuffer);
    }
    
    endRenderCommandAndPass(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

    //在命令缓冲区结束后需要发起的fence
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
    
    m_currentFrame = (m_currentFrame + 1) % m_swapchainImageCount;
}

void Application::clear()
{
    if(m_pUi)
    {
        delete m_pUi;
        m_pUi = nullptr;
    }
    
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    
    for(size_t i = 0; i < m_swapchainImageCount; ++i)
    {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    
    for(const auto& frameBuffer : m_framebuffers)
    {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }
    
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthMemory, nullptr);
    
    vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
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

void Application::mouse(double x, double y)
{
//    std::cout << x << std::endl;
//    std::cout << y << std::endl;
}

void Application::keyboard(int key, int scancode, int action, int mods)
{
    // action == GLFW_RELEASE, GLFW_PRESS, GLFW_REPEAT
    // mods == GLFW_MOD_SHIFT  GLFW_MOD_CONTROL GLFW_MOD_ALT GLFW_MOD_SUPER
    if(action != GLFW_RELEASE) return ;
    
    if(key == GLFW_KEY_ESCAPE)
    {
        std::cout << "escape" << std::endl;
    }
    else if(key == GLFW_KEY_1)
    {
        m_camera.m_moveAxis = 1;
    }
    else if(key == GLFW_KEY_2)
    {
        m_camera.m_moveAxis = 2;
    }
    else if(key == GLFW_KEY_3)
    {
        m_camera.m_moveAxis = 3;
    }
    else if(key == GLFW_KEY_4)
    {
        m_camera.m_moveAxis = 4;
    }
    else if(key == GLFW_KEY_5)
    {
        m_camera.m_moveAxis = 5;
    }
    else if(key == GLFW_KEY_6)
    {
        m_camera.m_moveAxis = 6;
    }
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
    
//    auto extensions = glfwGetRequiredInstanceExtensions(&propertiesCount);
    
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
    VkResult result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surfaceKHR);
    
    if( result != VK_SUCCESS )
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
    
    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProperties);
//    std::cout << m_deviceFeatures.fillModeNonSolid << std::endl;
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
    createInfo.pEnabledFeatures = &m_deviceEnabledFeatures;
    
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
        Tools::createImageView(m_swapchainImages[i], m_surfaceFormatKHR.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_swapchainImageViews[i]);
    }
}

void Application::createDepthBuffer()
{
    Tools::createImageAndMemoryThenBind(VK_FORMAT_D32_SFLOAT, m_swapchainExtent.width, m_swapchainExtent.height, 1, 1,
                                 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                 VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 m_depthImage, m_depthMemory);
    
    Tools::createImageView(m_depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_depthImageView);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Tools::setImageLayout(cmd, m_depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);

//    transitionImageLayout(m_depthImage, VK_FORMAT_D32_SFLOAT, , , 1);
}

void Application::createOtherBuffer()
{}

void Application::createDescriptorSetLayout(const VkDescriptorSetLayoutBinding* pBindings, uint32_t bindingCount)
{
    VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(pBindings, bindingCount);
    
    if ( vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create descriptorSetLayout!");
    }
}

void Application::createDescriptorPool(const VkDescriptorPoolSize* pPoolSizes, uint32_t poolSizeCount, uint32_t maxSets)
{
    VkDescriptorPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.poolSizeCount = poolSizeCount;
    createInfo.pPoolSizes = pPoolSizes;
    createInfo.maxSets = maxSets;

    if( vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create descriptorPool!");
    }
}

void Application::createDescriptorSet(VkDescriptorSet& descriptorSet)
{
//    VkDescriptorSetAllocateInfo allocInfo = {};
//    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//    allocInfo.descriptorPool = m_descriptorPool;
//    allocInfo.descriptorSetCount = 1;
//    allocInfo.pSetLayouts = &m_descriptorSetLayout;
//
//    if( vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet) != VK_SUCCESS)
//    {
//        throw std::runtime_error("failed to allocate descriptorSets!");
//    }
    
    createDescriptorSet(&m_descriptorSetLayout, 1, descriptorSet);
}

void Application::createDescriptorSet(const VkDescriptorSetLayout* pSetLayout, uint32_t descriptorSetCount, VkDescriptorSet& descriptorSet)
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = descriptorSetCount;
    allocInfo.pSetLayouts = pSetLayout;

    if( vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptorSets!");
    }
}

void Application::createPipelineLayout(const VkPushConstantRange* pPushConstantRange, uint32_t pushConstantRangeCount)
{
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &m_descriptorSetLayout;
    createInfo.pushConstantRangeCount = pushConstantRangeCount;
    createInfo.pPushConstantRanges = pPushConstantRange;

    if( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create layout!");
    }
}

void Application::createPipelineLayout(const VkDescriptorSetLayout* pSetLayout, uint32_t setLayoutCount, VkPipelineLayout& pipelineLayout)
{
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.setLayoutCount = setLayoutCount;
    createInfo.pSetLayouts = pSetLayout;
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges = nullptr;

    if( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &pipelineLayout) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create layout!");
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

void Application::createAttachmentDescription()
{
    VkAttachmentDescription depthAttachmentDescription = Tools::getAttachmentDescription(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    VkAttachmentDescription colorAttachmentDescription = Tools::getAttachmentDescription(m_surfaceFormatKHR.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    
    m_attachmentDescriptions.push_back(colorAttachmentDescription);
    m_attachmentDescriptions.push_back(depthAttachmentDescription);
}

void Application::createRenderPass()
{
    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
        
    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.attachmentCount = static_cast<uint32_t>(m_attachmentDescriptions.size());
    createInfo.pAttachments = m_attachmentDescriptions.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = 0;
    createInfo.pDependencies = nullptr;
    
    if( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create renderpass!");
    }
}

std::vector<VkImageView> Application::getAttachmentsImageViews(size_t i)
{
    std::vector<VkImageView> attachments = {};
    attachments.push_back(m_swapchainImageViews[i]);
    attachments.push_back(m_depthImageView);
    return attachments;
}

void Application::createFramebuffers()
{
    m_framebuffers.resize(m_swapchainImageViews.size());
    
    for(size_t i = 0; i < m_swapchainImageViews.size(); ++i)
    {
        std::vector<VkImageView> attachments = getAttachmentsImageViews(i);
        
        VkFramebufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = m_renderPass;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = m_swapchainExtent.width;
        createInfo.height = m_swapchainExtent.height;
        createInfo.layers = 1;
        
        if( vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create frame buffer!");
        }
    }
}

void Application::createCommandBuffers()
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

void Application::createSemaphores()
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
        if( (vkCreateSemaphore(m_device, &createInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) |
            (vkCreateSemaphore(m_device, &createInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS) |
            (vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]) ))
        {
            throw std::runtime_error("failed to create semaphorses!");
        }
    }
}

std::vector<VkClearValue> Application::getClearValue()
{
    std::vector<VkClearValue> clearValues = {};
    VkClearValue color = {};
    color.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    
    VkClearValue depth = {};
    depth.depthStencil = {1.0f, 0};
    
    clearValues.push_back(color);
    clearValues.push_back(depth);
    return clearValues;
}

void Application::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
}

void Application::beginRenderCommandAndPass(const VkCommandBuffer commandBuffer, int frameBufferIndex)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    if( vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to begin command buffer!");
    }
    
    createOtherRenderPass(commandBuffer);

    std::vector<VkClearValue> clearValues = getClearValue();
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_renderPass;
    passBeginInfo.framebuffer = m_framebuffers[frameBufferIndex];
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent = m_swapchainExtent;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void Application::endRenderCommandAndPass(const VkCommandBuffer commandBuffer)
{
    vkCmdEndRenderPass(commandBuffer);

    if( vkEndCommandBuffer(commandBuffer) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to end command buffer!");
    }
}


// -- helper function --

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

void Application::initUi()
{
    if(m_pUi == nullptr)
    {
        m_pUi = new Ui();
        m_pUi->m_device = m_device;
        m_pUi->m_graphicsQueue = m_graphicsQueue;
        m_pUi->m_width = m_width*2;
        m_pUi->m_height = m_height*2;
        m_pUi->m_title = m_title;
        m_pUi->prepareResources();
        m_pUi->preparePipeline(m_pipelineCache, m_renderPass);
    }
    
//    if (settings.overlay) {
//            UIOverlay.device = vulkanDevice;
//            UIOverlay.queue = queue;
//            UIOverlay.shaders = {
//                loadShader(getShadersPath() + "base/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
//                loadShader(getShadersPath() + "base/uioverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
//            };
//            UIOverlay.prepareResources();
//            UIOverlay.preparePipeline(pipelineCache, renderPass);
//        }
    
}

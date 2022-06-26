
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "ui.h"
#include "tools.h"

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    
    bool isComplete(){
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class Application
{
public:
    Application(std::string title);
    virtual ~Application();

    virtual void init();
    virtual void clear();
    void run();
    void loop();
    void logic();
    void render();
    
    void beginRenderCommandAndPass(const VkCommandBuffer commandBuffer, int frameBufferIndex);
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer) = 0;
    void endRenderCommandAndPass(const VkCommandBuffer commandBuffer);
    
    void resize(int width, int height);
    
protected:
    void createWindow();
    void createInstance();
    void createSurface();
    void choosePhysicalDevice();
    void createLogicDeivce();
    void createSwapchain();
    void createSwapchainImageView();
    
    void createDepthBuffer();
    
    void createPipelineCache();
    void createCommandPool();
    void createCommandBuffers();
    void createRenderPass();
    void createFramebuffers();
    void createDescriptorPool();
    
    void createSemaphores();
    
protected:
    void initUi();
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t LodLevel);
    QueueFamilyIndices findQueueFamilyIndices();
    
protected:
    GLFWwindow* m_window;
    int m_width = 400;
    int m_height = 300;
    std::string m_title;
    Ui* m_pUi = nullptr;
    
    std::chrono::steady_clock::time_point m_lastTimestamp;
    float m_averageDuration = 0.0f;
    uint32_t m_averageFPS = 0;

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
    uint32_t m_swapchainImageCount = 3;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    
    VkImage m_depthImage;
    VkDeviceMemory m_depthMemory;
    VkImageView m_depthImageView;
    
    VkCommandPool m_commandPool;
    VkDescriptorPool m_descriptorPool;
    VkPipelineCache m_pipelineCache;
    VkRenderPass m_renderPass;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkFence> m_inFlightFences;
    
    uint32_t m_currentFrame = 0;
};


#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "ui.h"
#include "tools.h"
#include "camera.h"

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
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void setSampleCount();
    virtual void clear();
    void run();
    void loop();
    void logic();
    void render();
    virtual void betweenInitAndLoop();
    virtual void updateRenderData();
    void beginRenderCommandAndPass(const VkCommandBuffer commandBuffer, int frameBufferIndex);
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer) = 0;
    void endRenderCommandAndPass(const VkCommandBuffer commandBuffer);
    virtual void queueResult();
    
    virtual void keyboard(int key, int scancode, int action, int mods);
    virtual void mouse(double x, double y);
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
    virtual void createOtherBuffer();
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    
    void createDescriptorSetLayout(const VkDescriptorSetLayoutBinding* pBindings, uint32_t bindingCount);
    void createDescriptorPool(const VkDescriptorPoolSize* pPoolSizes, uint32_t poolSizeCount, uint32_t maxSets);
    void createDescriptorSet(VkDescriptorSet& descriptorSet);
    void createDescriptorSet(const VkDescriptorSetLayout* pSetLayout, uint32_t descriptorSetCount, VkDescriptorSet& descriptorSet);
    void createPipelineLayout(const VkPushConstantRange* pPushConstantRange = nullptr, uint32_t pushConstantRangeCount = 0);
    void createPipelineLayout(const VkDescriptorSetLayout* pSetLayout, uint32_t setLayoutCount, VkPipelineLayout& pipelineLayout, const VkPushConstantRange* pPushConstantRange = nullptr, uint32_t pushConstantRangeCount = 0);
    
    void createPipelineCache();
    void createCommandPool();
    void createCommandBuffers();
    virtual void createAttachmentDescription();
    virtual void createRenderPass();
    void createFramebuffers();
    void createSemaphores();
    
    VkFormat findDepthFormat();
    virtual std::vector<VkImageView> getAttachmentsImageViews(size_t i);
    virtual std::vector<VkClearValue> getClearValue();
    
protected:
    void initUi();
    QueueFamilyIndices findQueueFamilyIndices();
    
protected:
    GLFWwindow* m_window;
    int m_width = 1280;
    int m_height = 720;
    std::string m_title;
    Ui* m_pUi = nullptr;
    Camera m_camera;

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
    
    VkPhysicalDeviceProperties m_deviceProperties;
    VkPhysicalDeviceFeatures m_deviceFeatures;
    VkPhysicalDeviceMemoryProperties m_deviceMemoryProperties;
    VkPhysicalDeviceFeatures m_deviceEnabledFeatures = {}; //上面是总的特征,这个是程序支持的特征.
    
    VkImageUsageFlags m_swapchainImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    VkSwapchainKHR m_swapchainKHR;
    VkExtent2D m_swapchainExtent;
    VkSurfaceFormatKHR m_surfaceFormatKHR;
    uint32_t m_swapchainImageCount = 3;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    
    VkSampleCountFlagBits m_sampleCount = VK_SAMPLE_COUNT_1_BIT;
    
    VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_depthImage;
    VkDeviceMemory m_depthMemory;
    VkImageView m_depthImageView;
    
    VkCommandPool m_commandPool;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineCache m_pipelineCache;
    VkPipelineLayout m_pipelineLayout;
    
    std::vector<VkAttachmentDescription> m_attachmentDescriptions;
    VkRenderPass m_renderPass;
    VkSubpassContents m_subpassContents = VK_SUBPASS_CONTENTS_INLINE;
    
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkFence> m_inFlightFences;
    
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    
    
};


#pragma once

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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
    void run();
    virtual void render();
    void loop();
    virtual void clear();
    void resize(int width, int height);
    
    
protected:
    void createWindow();
    void createInstance();
    void createSurface();
    void choosePhysicalDevice();
    void createLogicDeivce();
    void createSwapchain();
    void createSwapchainImageView();
    
    void createPipelineCache();
    void createCommandPool();
    
protected:
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t LodLevel);
    QueueFamilyIndices findQueueFamilyIndices();
    VkShaderModule createShaderModule(const std::string& filename);
    std::vector<char> readFile(const std::string& filename);
    
protected:
    GLFWwindow* m_window;
    int m_width = 400;
    int m_height = 300;
    std::string m_title;

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
    
    VkCommandPool m_commandPool;
    VkPipelineCache m_pipelineCache;
};

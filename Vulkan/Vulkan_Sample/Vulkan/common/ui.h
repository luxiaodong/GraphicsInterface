
#pragma once

#include "imgui.h"
#include "tools.h"

struct PushConstBlock
{
    glm::vec2 scale;
    glm::vec2 translate;
};

class Ui
{
public:
    Ui();
    ~Ui();
    void prepareResources();
    void preparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass);
    
public:
    void recordRenderCommand(const VkCommandBuffer commandBuffer);
    void updateUI(uint32_t lastFPS);
    bool updateRenderData();
    
protected:
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createPipelineLayout();

public:
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    int m_width;
    int m_height;
    std::string m_title;
    
private:
    VkImage m_fontImage = VK_NULL_HANDLE;
    VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
    VkImageView m_fontImageView = VK_NULL_HANDLE;
    VkSampler m_fontSampler = VK_NULL_HANDLE;
    
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexMemory = VK_NULL_HANDLE;
    VkDeviceSize m_vertexBufferSize = 0;
    VkDeviceSize m_indexBufferSize = 0;
    
    PushConstBlock m_pushConstBlock;
    
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
};



#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class Bloom : public Application
{
public:
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
    };
    
    struct BlurParams {
        float blurScale = 1.0f;
        float blurStrength = 1.5f;
    };
    
    Bloom(std::string title);
    virtual ~Bloom();
    
    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();
    
    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    
protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();

protected:
    void createOffscreenRenderPass();
    void createOffscreenFrameBuffer();
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();

protected:
    VkBuffer m_skyboxUniformBuffer;
    VkDeviceMemory m_skyboxUniformMemory;
    VkBuffer m_objectUniformBuffer;
    VkDeviceMemory m_objectUniformMemory;
    VkBuffer m_blurParamUniformBuffer;
    VkDeviceMemory m_blurParamUniformMemory;
    
    uint32_t m_offscrrenWidth = 1024;
    uint32_t m_offscrrenHeight = 1024;
    VkFormat m_offscreenColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImage m_offscreenColorImage[2];
    VkDeviceMemory m_offscreenColorMemory[2];
    VkImageView m_offscreenColorImageView[2];
    VkSampler m_offscreenColorSample;
    VkFormat m_offscreenDepthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_offscreenDepthImage[2];
    VkDeviceMemory m_offscreenDepthMemory[2];
    VkImageView m_offscreenDepthImageView[2];
    
    VkRenderPass m_offscreenRenderPass;
    VkFramebuffer m_offscreenFrameBuffer[2];
    
    VkDescriptorSetLayout m_blurDescriptorSetLayout;
    VkPipelineLayout m_blurPipelineLayout;
    
    VkDescriptorSet m_skyboxDescriptorSet;
    VkDescriptorSet m_objectDescriptorSet;
    VkDescriptorSet m_blurDescriptorSet[2];
    
    VkPipeline m_bloomPipeline[2];
    VkPipeline m_skyboxPipeline;
    VkPipeline m_objectPipeline;
    VkPipeline m_glowPipeline;

private:
    Texture* m_pTexture = nullptr;
    GltfLoader m_objectLoader;
    GltfLoader m_skyboxLoader;
    GltfLoader m_glowLoader;
};

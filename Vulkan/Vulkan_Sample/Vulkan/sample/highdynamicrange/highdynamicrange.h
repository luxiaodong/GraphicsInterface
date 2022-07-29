
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class HighDynamicRange : public Application
{
public:
    
    struct Vertex {
        float position[3];
        float uv[2];
        float color[3];
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 invViewMatrix;
    };
    
    struct Exposure {
        float exposure = 1.0f;
    };
    
    HighDynamicRange(std::string title);
    virtual ~HighDynamicRange();
    
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
    // skybox & object.
    VkBuffer m_skyboxUniformBuffer;
    VkDeviceMemory m_skyboxUniformMemory;
    VkBuffer m_exposureUniformBuffer;
    VkDeviceMemory m_exposureUniformMemory;
    
    VkDescriptorSetLayout m_skyboxDescriptorSetLayout;
    VkPipelineLayout m_skyboxPipelineLayout;
    VkDescriptorSet m_skyboxDescriptorSet;
    VkPipeline m_skyboxPipeline;
    VkPipeline m_objectPipeline;
    
    // 3份 attachment.
    VkFormat m_offscreenColorFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkImage m_offscreenColorImage[2];
    VkDeviceMemory m_offscreenColorMemory[2];
    VkImageView m_offscreenColorImageView[2];
    VkSampler m_offscreenColorSample;
    
    VkFormat m_offscreenDepthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_offscreenDepthImage;
    VkDeviceMemory m_offscreenDepthMemory;
    VkImageView m_offscreenDepthImageView;
    
    VkRenderPass m_offscreenRenderPass;
    VkFramebuffer m_offscreenFrameBuffer;
    
    // composition
    VkPipeline m_compositionPipeline;
    VkDescriptorSet m_compositionDescriptorSet;
    
private:
    Texture* m_pTexture = nullptr;
    GltfLoader m_objectLoader;
    GltfLoader m_skyboxLoader;
};

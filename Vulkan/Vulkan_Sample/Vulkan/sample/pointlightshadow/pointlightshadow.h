
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class PointLightShadow : public Application
{
public:
    struct PushConstantData
    {
        glm::mat4 viewMatrix;
    };
    
    struct Uniform
    {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 lightPos;
    };
    
    PointLightShadow(std::string title);
    virtual ~PointLightShadow();
    
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

    void updateShadowMapMVP();
    void updateUniformMVP();
    
protected:
    // shadowmapping.
    glm::vec4 m_lightPos = glm::vec4(0.0f, -2.5f, 0.0f, 1.0);
    float m_zNear = 0.1f;
    float m_zFar = 512.0f;
    
    uint32_t m_shadowMapWidth = 1024;
    uint32_t m_shadowMapHeight = 1024;
    VkBuffer m_shadowUniformBuffer;
    VkDeviceMemory m_shadowUniformMemory;
    
    VkPushConstantRange m_shadowPushConstantRange;
    VkDescriptorSetLayout m_shadowDescriptorSetLayout;
    VkPipelineLayout m_shadowPipelineLayout;
    VkDescriptorSet m_shadowDescriptorSet;
    VkPipeline m_shadowPipeline;
    
    // shadow map attachment.
    VkFormat m_offscreenColorFormat = VK_FORMAT_R32_SFLOAT;
    VkImage m_offscreenColorImage;
    VkDeviceMemory m_offscreenColorMemory;
    VkImageView m_offscreenColorImageView;
    VkFormat m_offscreenDepthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_offscreenDepthImage;
    VkDeviceMemory m_offscreenDepthMemory;
    VkImageView m_offscreenDepthImageView;
    
    VkRenderPass m_offscreenRenderPass;
    VkFramebuffer m_offscreenFrameBuffer;
    
    // cube
    VkImage m_cubeImage;
    VkDeviceMemory  m_cubeMemory;
    VkImageView     m_cubeImageView;
    VkSampler m_cubeSampler;
    
    // debug
    bool m_isShowDebug = false;
    VkPipeline m_debugPipeline;
    
    // scene
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;

private:
    GltfLoader m_sceneLoader;
    GltfLoader m_cubeLoader;
};

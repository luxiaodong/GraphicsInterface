
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class ShadowMapping : public Application
{
public:
    struct ShadowUniform {
        glm::mat4 shadowMVP;
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 shadowMapMvp;
        glm::vec4 lightPos;
        float zNear;
        float zFar;
    };
    
    ShadowMapping(std::string title);
    virtual ~ShadowMapping();
    
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

    void updateLightPos();
    void updateShadowMapMVP();
    void updateUniformMVP();
    
protected:
    // shadowmapping.
    glm::vec3 m_lightPos = glm::vec3(0.0f);
    float m_lightFOV = 45.0f;
    float m_zNear = 1.0f;
    float m_zFar = 96.0f;
    
    uint32_t m_shadowMapWidth = 1024;
    uint32_t m_shadowMapHeight = 1024;
    VkBuffer m_shadowUniformBuffer;
    VkDeviceMemory m_shadowUniformMemory;
    ShadowUniform m_shadowUniformMvp;
    
    VkDescriptorSetLayout m_shadowDescriptorSetLayout;
    VkPipelineLayout m_shadowPipelineLayout;
    VkDescriptorSet m_shadowDescriptorSet;
    VkPipeline m_shadowPipeline;
    
    // shadow map attachment.
    VkFormat m_offscreenDepthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_offscreenDepthImage;
    VkDeviceMemory m_offscreenDepthMemory;
    VkImageView m_offscreenDepthImageView;
    VkSampler m_offscreenDepthSampler;
    
    VkRenderPass m_offscreenRenderPass;
    VkFramebuffer m_offscreenFrameBuffer;
    
    // debug & scene
    VkPipeline m_debugPipeline;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;

private:
    GltfLoader m_sceneLoader;
};

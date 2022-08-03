
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

#define SHADOW_MAP_CASCADE_COUNT 4

class ShadowMappingCascade : public Application
{
public:
    struct ShadowUniform {
        std::array<glm::mat4, SHADOW_MAP_CASCADE_COUNT> cascadeVP;
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
    
    struct Cascade
    {
        float splitDepth;
        glm::mat4 viewProjMatrix;
        
        VkFramebuffer frameBuffer;
        VkDescriptorSet descriptorSet;
        VkImageView view;
    };
    
    struct PushConstantData
    {
        glm::vec4 position;
        uint32_t cascadeIndex;
    };

    ShadowMappingCascade(std::string title);
    virtual ~ShadowMappingCascade();
    
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
    void updateCascades();
    void updateShadowMapMVP();
    void updateUniformMVP();
    
protected:
    // shadowmapping.
    glm::vec3 m_lightPos = glm::vec3(0.0f);
    float m_zNear = 0.5f;
    float m_zFar = 48.0f;
    std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> m_cascades;
    
    
    uint32_t m_shadowMapWidth = 1024;
    uint32_t m_shadowMapHeight = 1024;
    VkBuffer m_shadowUniformBuffer;
    VkDeviceMemory m_shadowUniformMemory;
    ShadowUniform m_shadowUniformMvp;
    
    VkPushConstantRange m_shadowPushConstantRange;
    VkDescriptorSetLayout m_shadowDescriptorSetLayout;
    VkPipelineLayout m_shadowPipelineLayout;
//    VkDescriptorSet m_shadowDescriptorSet;
    VkPipeline m_shadowPipeline;
    
    // shadow map attachment.
    VkFormat m_offscreenDepthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage m_offscreenDepthImage;
    VkDeviceMemory m_offscreenDepthMemory;
    VkImageView m_offscreenDepthImageView;
    VkSampler m_offscreenDepthSampler;
    
    VkRenderPass m_offscreenRenderPass;
    
    // debug
    VkDescriptorSetLayout m_debugDescriptorSetLayout;
    VkPipelineLayout m_debugPipelineLayout;
    VkDescriptorSet m_debugDescriptorSet;
    VkPipeline m_debugPipeline;
    
    // scene
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;

private:
    GltfLoader m_terrainLoader;
    GltfLoader m_treeLoader;
};

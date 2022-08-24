
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class DeferredShadows : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 instancePos[3];
        int layer;
    };
    
    struct ShadowUniform {
        glm::mat4 shadowMvp[3];
        glm::vec4 instancePos[3];
    };
    
    struct Light {
        glm::vec4 position;
        glm::vec4 target;
        glm::vec4 color;
        glm::mat4 shadowMvp;
    };
    
    struct LightData
    {
        glm::vec4 viewPos;
        Light lights[3]; // 支持3盏灯
        uint32_t useShadows = 1;
        int debugDisplayTarget = 0;
    };
    
    DeferredShadows(std::string title);
    virtual ~DeferredShadows();
    
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
    void createDeferredRenderPass();
    void createDeferredFrameBuffer();
    void createShadowMapRenderPass();
    void createShadowMapFrameBuffer();
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();
    void createGBufferRenderPass(const VkCommandBuffer& commandBuffer);
    void createShadowMapRenderPass(const VkCommandBuffer& commandBuffer);

protected:
    // 4份 attachment. positon, normal, albedoo, depth
    uint32_t m_deferredWidth;
    uint32_t m_deferredHeight;
    VkFramebuffer m_deferredFramebuffer;
    VkRenderPass m_deferredRenderPass;
    
    VkFormat m_deferredColorFormat[3];
    VkImage m_deferredColorImage[3];
    VkDeviceMemory m_deferredColorMemory[3];
    VkImageView m_deferredColorImageView[3];
    
    VkFormat m_deferredDepthFormat;
    VkImage m_deferredDepthImage;
    VkDeviceMemory m_deferredDepthMemory;
    VkImageView m_deferredDepthImageView;
    
    VkSampler m_deferredColorSample;
    
    // shadowmap
    uint32_t m_shadowMapWidth = 1024;
    uint32_t m_shadowMapHeight = 1024;
    VkFramebuffer m_shadowMapFramebuffer;
    VkRenderPass m_shadowMapRenderPass;
    
    VkFormat m_shadowMapFormat;
    VkImage m_shadowMapImage;
    VkDeviceMemory m_shadowMapMemory;
    VkImageView m_shadowMapImageView;
    VkSampler m_shadowMapSample;

    //mrt
    VkPipeline m_mrtPipeline;
    VkBuffer m_mrtUniformBuffer;
    VkDeviceMemory m_mrtUniformMemory;
    VkDescriptorSetLayout m_mrtDescriptorSetLayout;
    VkPipelineLayout m_mrtPipelineLayout;
    
    VkDescriptorSet m_floorDescriptorSet;
    VkDescriptorSet m_objectDescriptorSet;
    
    //shadow
    VkPipeline m_shadowMapPipeline;
    VkBuffer m_shadowMapUniformBuffer;
    VkDeviceMemory m_shadowMapUniformMemory;
    VkDescriptorSetLayout m_shadowMapDescriptorSetLayout;
    VkPipelineLayout m_shadowMapPipelineLayout;
    VkDescriptorSet m_shadowMapDescriptorSet;
    
    //deferred
    VkPipeline m_pipeline;
    VkBuffer m_lightUniformBuffer;
    VkDeviceMemory m_lightUniformMemory;
    VkDescriptorSet m_descriptorSet;
    
private:
    GltfLoader m_objectLoader;
    GltfLoader m_floorLoader;
    
    Texture* m_pObjectColor = nullptr;
    Texture* m_pObjectNormal = nullptr;
    Texture* m_pFloorColor = nullptr;
    Texture* m_pFloorNormal = nullptr;
};

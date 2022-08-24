
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class DeferredMutiSampling : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 instancePos[3];
    };
    
    struct Light {
        glm::vec4 position;
        glm::vec3 color;
        float radius;
    };
    
    struct LightData
    {
        Light lights[6];
        glm::vec4 viewPos;
        int debugDisplayTarget = 0;
    };
    
    DeferredMutiSampling(std::string title);
    virtual ~DeferredMutiSampling();
    
    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void setSampleCount();
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
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();

protected:
    // 4份 attachment. positon, normal, albedoo, depth
    uint32_t m_gbufferWidth;
    uint32_t m_gbufferHeight;
    VkFramebuffer m_gbufferFramebuffer;
    VkRenderPass m_gbufferRenderPass;
    VkPipeline m_gbufferPipeline;
    
    VkFormat m_gbufferColorFormat[3];
    VkImage m_gbufferColorImage[3];
    VkDeviceMemory m_gbufferColorMemory[3];
    VkImageView m_gbufferColorImageView[3];
    
    VkFormat m_gbufferDepthFormat;
    VkImage m_gbufferDepthImage;
    VkDeviceMemory m_gbufferDepthMemory;
    VkImageView m_gbufferDepthImageView;
    
    VkSampler m_gbufferColorSample;
    VkSampleCountFlagBits m_deferredSampleCount = VK_SAMPLE_COUNT_1_BIT;

    //mrt.
    VkPipeline m_mrtPipeline;
    VkBuffer m_mrtUniformBuffer;
    VkDeviceMemory m_mrtUniformMemory;
    VkDescriptorSetLayout m_mrtDescriptorSetLayout;
    VkPipelineLayout m_mrtPipelineLayout;
    
    VkDescriptorSet m_floorDescriptorSet;
    VkDescriptorSet m_objectDescriptorSet;
    
    //
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

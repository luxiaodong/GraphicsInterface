
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class DeferredSsao : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        float near = 0.1f;
        float far = 64.0f;
    };
    
    struct SsaoParams {
        glm::mat4 projection;
        int32_t ssao = true;
        int32_t ssaoOnly = false;
        int32_t ssaoBlur = true;
    };
    
    DeferredSsao(std::string title);
    virtual ~DeferredSsao();
    
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
    void createSsaoRenderPass();
    void createSsaoFrameBuffer();
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();
    
    void createGbufferRenderPass(const VkCommandBuffer& commandBuffer);
    void createSsaoRenderPass(const VkCommandBuffer& commandBuffer);
    
protected:
    // 4份 attachment. positon, normal, albedoo, depth
    uint32_t m_gbufferWidth;
    uint32_t m_gbufferHeight;
    VkFramebuffer m_gbufferFramebuffer;
    VkRenderPass m_gbufferRenderPass;
    
    VkFormat m_gbufferColorFormat[3];
    VkImage m_gbufferColorImage[3];
    VkDeviceMemory m_gbufferColorMemory[3];
    VkImageView m_gbufferColorImageView[3];
    
    VkFormat m_gbufferDepthFormat;
    VkImage m_gbufferDepthImage;
    VkDeviceMemory m_gbufferDepthMemory;
    VkImageView m_gbufferDepthImageView;
    
    VkSampler m_gbufferColorSample;

    // gbuffer
    VkPipeline m_gbufferPipeline;
    VkDescriptorSetLayout m_gbufferDescriptorSetLayout;
    VkPipelineLayout m_gbufferPipelineLayout;
    
    // ssao
    VkPipeline m_ssaoPipeline;
    VkDescriptorSetLayout m_ssaoDescriptorSetLayout;
    VkPipelineLayout m_ssaoPipelineLayout;
    
    VkFramebuffer m_ssaoFramebuffer;
    VkRenderPass m_ssaoRenderPass;
    
    VkFormat m_ssaoColorFormat;
    VkImage m_ssaoColorImage;
    VkDeviceMemory m_ssaoColorMemory;
    VkImageView m_ssaoColorImageView;
    VkSampler m_ssaoColorSample;
    
    // buffer
    VkBuffer m_objectUniformBuffer;
    VkDeviceMemory m_objectUniformMemory;
    VkBuffer m_paramsUniformBuffer;
    VkDeviceMemory m_paramsUniformMemory;
    VkBuffer m_sampleUniformBuffer;
    VkDeviceMemory m_sampleUniformMemory;
    
    // pipeline
    VkPipeline m_pipeline;
    VkDescriptorSet m_descriptorSet;
    VkDescriptorSet m_objectDescriptorSet;
    VkDescriptorSet m_ssaoDescriptorSet;
    
private:
    GltfLoader m_objectLoader;
    Texture* m_pNoise = nullptr;
};

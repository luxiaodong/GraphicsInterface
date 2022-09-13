
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class RadialBlur : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        float gradientPos = 0.0f;
    };
    
    struct BlurParams {
        float radialBlurScale = 0.35f;
        float radialBlurStrength = 0.75f;
        glm::vec2 radialOrigin = glm::vec2(0.5f, 0.5f);
    };
    
    RadialBlur(std::string title);
    virtual ~RadialBlur();
    
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
    
    void createOffScreenRenderPass();
    void createOffScreenFrameBuffer();
    
protected:
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();
//    virtual void createAttachmentDescription();
//    virtual void createRenderPass();

protected:
    // offscreen
    uint32_t m_offScreenWidth = 512;
    uint32_t m_offScreenHeight = 512;

    VkPipeline m_offScreenPipeline;
    VkBuffer m_offScreenUniformBuffer;
    VkDeviceMemory m_offScreenUniformMemory;
    
    VkRenderPass m_offScreenRenderPass;
    VkFramebuffer m_offScreenFrameBuffer;
    VkImage m_offScreenColorImage;
    VkDeviceMemory m_offScreenColorMemory;
    VkImageView m_offScreenColorImageView;
    VkImage m_offScreenDepthImage;
    VkDeviceMemory m_offScreenDepthMemory;
    VkImageView m_offScreenDepthImageView;
    VkSampler m_offScreenColorSampler;
    
    // pipeline
    VkPipeline m_phongPipeline;
    VkPipeline m_colorPipeline;
    VkPipeline m_radialBlurPipeline;
    
    //descriptorSet
    VkDescriptorSet m_radialBlurDescriptorSet;
    VkPipelineLayout m_radialBlurPipelineLayout;
    VkDescriptorSetLayout m_radialBlurDescriptorSetLayout;
    VkDescriptorSet m_objectDescriptorSet;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkBuffer m_blurParamsBuffer;
    VkDeviceMemory m_blurParamsMemory;

private:
    GltfLoader m_objectLoader;
    Texture* m_pGradient;
};

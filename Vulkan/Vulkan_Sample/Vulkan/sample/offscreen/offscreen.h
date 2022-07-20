
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class OffScreen : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    };
    
    OffScreen(std::string title);
    virtual ~OffScreen();
    
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
    
    void createMirrorRenderPass();
    void createMirrorFrameBuffer();
    
protected:
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();
//    virtual void createAttachmentDescription();
//    virtual void createRenderPass();

protected:
    //pass 1 mirror
    uint32_t m_mirrorWidth = 512;
    uint32_t m_mirrorHeight = 512;

    VkPipeline m_mirrorPipeline;
    VkDescriptorSet m_mirrorDescriptorSet;
    VkPipelineLayout m_mirrorPipelineLayout;
    VkDescriptorSetLayout m_mirrorDescriptorSetLayout;
    VkBuffer m_mirrorUniformBuffer;
    VkDeviceMemory m_mirrorUniformMemory;
    
    VkRenderPass m_mirrorRenderPass;
    VkFramebuffer m_mirrorFrameBuffer;
    VkImage m_mirrorColorImage;
    VkDeviceMemory m_mirrorColorMemory;
    VkImageView m_mirrorColorImageView;
    VkImage m_mirrorDepthImage;
    VkDeviceMemory m_mirrorDepthMemory;
    VkImageView m_mirrorDepthImageView;
    VkSampler m_mirrorColorSampler;
    
    //pass 2 debug
    VkPipeline m_debugPipeline;
    VkDescriptorSet m_debugDescriptorSet;
    VkPipelineLayout m_debugPipelineLayout;
    VkDescriptorSetLayout m_debugDescriptorSetLayout;
    Texture* m_pGlassTexture = nullptr; //玻璃材质
    
    //pass 3 plane
    //VkPipeline m_planePipeline;
    
    //pass 4 dragon
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
private:
    GltfLoader m_dragonLoader;
    GltfLoader m_planeLoader;
};

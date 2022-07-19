
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

#define NUM_LIGHTS 64

class SubPasses : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };
    
    struct Light {
        glm::vec4 position;
        glm::vec3 color;
        float radius;
    };
    
    struct ViewPosAndLights {
        glm::vec4 viewPos;
        Light lights[NUM_LIGHTS];
    };
    
    SubPasses(std::string title);
    virtual ~SubPasses();
    
    virtual void init();
    virtual void initCamera();
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
    virtual void createOtherBuffer();
    virtual void createAttachmentDescription();
    virtual void createRenderPass();
    virtual std::vector<VkImageView> getAttachmentsImageViews(size_t i);
    virtual std::vector<VkClearValue> getClearValue();
    
    //3个pass,1.生成gbuffer,2.使用gbuffer,3.叠加一个透明物体
private:
    // pass 1
    VkPipeline m_scenePipeline;
    VkDescriptorSetLayout m_sceneDescriptorSetLayout;
    VkPipelineLayout m_scenePipelineLayout;
    VkDescriptorSet m_sceneDescriptorSet;
    GltfLoader m_sceneLoader;
    VkBuffer m_sceneUniformBuffer;
    VkDeviceMemory m_sceneUniformMemory;
    
    // 3组buffer.
    VkFormat m_positionFormat;
    VkImage m_positionImage;
    VkDeviceMemory m_positionMemory;
    VkImageView m_positionImageView;
    
    VkFormat m_normalFormat;
    VkImage m_normalImage;
    VkDeviceMemory m_normalMemory;
    VkImageView m_normalImageView;
    
    VkFormat m_albedoFormat;
    VkImage m_albedoImage;
    VkDeviceMemory m_albedoMemory;
    VkImageView m_albedoImageView;
    
    // pass 2
    VkPipeline m_compositePipeline;
    VkDescriptorSetLayout m_compositeDescriptorSetLayout;
    VkPipelineLayout m_compositePipelineLayout;
    VkDescriptorSet m_compositeDescriptorSet;
    VkBuffer m_lightUniformBuffer;
    VkDeviceMemory m_lightUniformMemory;
    
    // pass 3
    // uniformBuffer使用pass1的. VkPipelineLayout和VkDescriptorSetLayout用基类的
    VkPipeline m_transparentPipeline;
    VkDescriptorSet m_transparentDescriptorSet;
    GltfLoader m_transparentLoader;
    Texture* m_pGlassTexture = nullptr; //玻璃材质
};

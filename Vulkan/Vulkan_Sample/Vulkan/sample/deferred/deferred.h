
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class Deferred : public Application
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
    
    Deferred(std::string title);
    virtual ~Deferred();
    
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
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();

protected:
    // 4份 attachment. positon, normal, albedoo, depth
    uint32_t m_deferredWidth;
    uint32_t m_deferredHeight;
    VkFramebuffer m_deferredFramebuffer;
    VkRenderPass m_deferredRenderPass;
    VkPipeline m_deferredPipeline;
    
    VkFormat m_deferredColorFormat[3];
    VkImage m_deferredColorImage[3];
    VkDeviceMemory m_deferredColorMemory[3];
    VkImageView m_deferredColorImageView[3];
    
    VkFormat m_deferredDepthFormat;
    VkImage m_deferredDepthImage;
    VkDeviceMemory m_deferredDepthMemory;
    VkImageView m_deferredDepthImageView;
    
    VkSampler m_deferredColorSample;

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

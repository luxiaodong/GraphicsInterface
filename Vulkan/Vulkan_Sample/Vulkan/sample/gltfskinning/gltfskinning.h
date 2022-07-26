
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class GltfSkinning : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, 5.0f, 1.0f);
    };
    
    GltfSkinning(std::string title);
    virtual ~GltfSkinning();
    
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
    
    virtual std::vector<VkClearValue> getClearValue();

protected:
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    VkDescriptorSetLayout m_jointMatrixDescriptorSetLayout;
    VkDescriptorSetLayout m_textureDescriptorSetLayout;

private:
    GltfLoader m_gltfLoader;
};

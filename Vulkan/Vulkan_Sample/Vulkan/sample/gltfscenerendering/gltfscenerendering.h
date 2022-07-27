
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class GltfSceneRendering : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, 2.5f, 0.0f, 1.0f);
        glm::vec4 viewPos;
    };
    
    struct SpecializationData {
        VkBool32 alphaMask;
        float alphaMaskCutoff;
    };
    
    GltfSceneRendering(std::string title);
    virtual ~GltfSceneRendering();
    
    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();
    
    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    
protected:
    void prepareVertex();
    void prepareUniform();
    void prepareSpecializationInfo();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();

protected:
//    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    VkDescriptorSetLayout m_textureDescriptorSetLayout;

private:
    GltfLoader m_gltfLoader;
};

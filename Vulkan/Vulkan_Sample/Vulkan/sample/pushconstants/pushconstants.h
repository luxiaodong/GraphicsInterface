
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class PushConstants : public Application
{
public:
    struct SpherePushConstantData
    {
        glm::vec4 color;
        glm::vec4 position;
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };
    
    PushConstants(std::string title);
    virtual ~PushConstants();
    
    virtual void init();
    virtual void initCamera();
    virtual void clear();
    
    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    
protected:
    void prepareVertex();
    void prepareUniform();
    void preparePushConstants();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();

protected:
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    VkPushConstantRange m_pushConstantRange;

    std::array<SpherePushConstantData, 16> m_spheres;
private:
    GltfLoader m_gltfLoader;
};

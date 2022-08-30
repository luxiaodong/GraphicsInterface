
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class GeometryShader : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
    };
    
    GeometryShader(std::string title);
    virtual ~GeometryShader();
    
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
    VkPipeline m_normalPipeline;
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
private:
    GltfLoader m_objectLoader;
};

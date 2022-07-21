
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class ParticleFire : public Application
{
public:
    
    struct Vertex {
        float position[3];
        float color[3];
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 normalMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    };
    
    ParticleFire(std::string title);
    virtual ~ParticleFire();
    
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
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;

private:
    GltfLoader m_environmentLoader;
    Texture* m_pFloorColor;
    Texture* m_pFloorNormal;
};

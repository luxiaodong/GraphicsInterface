
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class TextureCubeMapping : public Application
{
public:
    
    struct Vertex {
        float position[3];
        float uv[2];
        float color[3];
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 invViewMatrix;
        float lodBias;
    };
    
    TextureCubeMapping(std::string title);
    virtual ~TextureCubeMapping();
    
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
    VkDescriptorSet m_skyboxDescriptorSet;
    VkDescriptorSet m_objectDescriptorSet;
    
    VkPipeline m_skyboxPipeline;
    VkPipeline m_objectPipeline;
    
    VkBuffer m_skyboxUniformBuffer;
    VkDeviceMemory m_skyboxUniformMemory;
    VkBuffer m_objectUniformBuffer;
    VkDeviceMemory m_objectUniformMemory;
    
    Texture* m_pTexture = nullptr;
private:
    GltfLoader m_objectLoader;
    GltfLoader m_skyboxLoader;
};

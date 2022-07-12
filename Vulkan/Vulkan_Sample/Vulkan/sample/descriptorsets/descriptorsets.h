
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class Descriptorsets : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };
    
    struct Cube
    {
        Uniform matrix;
        VkBuffer m_uniformBuffer;
        VkDeviceMemory m_uniformMemory;
        VkDescriptorSet descriptorSet;
        glm::vec3 rotation;
        Texture* pTextrue = nullptr;
    };

    Descriptorsets(std::string title);
    virtual ~Descriptorsets();

    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();

    virtual void prepareRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);

protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();
    
private:
    Cube m_cube;
    
    VkPipeline m_pipeline;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
        
private:
    GltfLoader m_gltfLoader;
};

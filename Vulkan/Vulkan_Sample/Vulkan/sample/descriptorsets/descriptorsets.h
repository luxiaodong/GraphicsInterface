
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
        glm::vec3 rotation;
        Texture* pTextrue = nullptr;
        VkBuffer uniformBuffer;
        VkDeviceMemory uniformMemory;
        VkDescriptorSet descriptorSet;
    };

    Descriptorsets(std::string title);
    virtual ~Descriptorsets();

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
    
private:
    Cube m_cube[2];
    VkPipeline m_pipeline;
        
private:
    GltfLoader m_gltfLoader;
};

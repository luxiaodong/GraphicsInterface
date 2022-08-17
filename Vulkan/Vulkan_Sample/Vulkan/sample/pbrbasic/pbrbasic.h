
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class PbrBasic : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        glm::vec3 camPos;
    };
    
    struct LightParams
    {
        glm::vec4 lights[4];
    };
    
    PbrBasic(std::string title);
    virtual ~PbrBasic();

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
    
    void selectPbrMaterial();

private:
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkBuffer m_lightBuffer;
    VkDeviceMemory m_lightMemory;
    
    VkPipeline m_pipeline;
    VkDescriptorSet m_descriptorSet;

    PbrMaterial m_pbrMaterial;
private:
    GltfLoader m_gltfLoader;
};

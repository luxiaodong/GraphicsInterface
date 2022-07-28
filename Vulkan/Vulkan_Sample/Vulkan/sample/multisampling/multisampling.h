
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"

class MultiSampling : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos = glm::vec4(5.0f, -5.0f, 5.0f, 1.0f);
    };

    MultiSampling(std::string title);
    virtual ~MultiSampling();

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
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;      //msaa

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    VkPipeline m_multiSamplingPipeline; //msaaSampling
    VkDescriptorSetLayout m_textureDescriptorSetLayout;

private:
    GltfLoader m_gltfLoader;
};

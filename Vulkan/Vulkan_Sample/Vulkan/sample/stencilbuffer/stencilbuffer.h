
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"

class StencilBuffer : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, -2.0f, 1.0f, 0.0f);
        float outlineWidth = 0.025f;
    };

    StencilBuffer(std::string title);
    virtual ~StencilBuffer();

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
    VkPipeline m_graphicsPipeline;
    VkPipeline m_outlinePipeline;

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;

private:
    GltfLoader m_gltfLoader;
};

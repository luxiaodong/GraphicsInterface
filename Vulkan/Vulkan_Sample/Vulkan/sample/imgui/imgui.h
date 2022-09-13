
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"
#include "common/text.h"

class ImGUI : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos;
    };

    ImGUI(std::string title);
    virtual ~ImGUI();

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

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;

private:
    GltfLoader m_backgroundLoader;
    GltfLoader m_gameobjectLoader;
    GltfLoader m_vulkanlogoLoader;
};

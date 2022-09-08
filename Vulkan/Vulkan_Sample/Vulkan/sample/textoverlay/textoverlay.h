
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"
#include "common/text.h"

class Textoverlay : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos;
    };

    Textoverlay(std::string title);
    virtual ~Textoverlay();

    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();

    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    virtual void createOtherRenderPass(const VkFramebuffer& frameBuffer);

protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();
    void prepareText();

protected:
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;

private:
    GltfLoader m_gltfLoader;
    Text* m_pText;
};

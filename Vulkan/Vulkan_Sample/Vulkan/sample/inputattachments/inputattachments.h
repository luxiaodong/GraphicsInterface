
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class InputAttachments : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };
    
    struct Params {
        glm::vec2 brightnessContrast = glm::vec2(0.5f, 1.8f);
        glm::vec2 range = glm::vec2(0.6f, 1.0f);
        int32_t attachmentIndex = 1;
    };
    
    InputAttachments(std::string title);
    virtual ~InputAttachments();
    
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
    virtual void createOtherBuffer();
    virtual void createAttachmentDescription();
    virtual void createRenderPass();
    virtual std::vector<VkImageView> getAttachmentsImageViews(size_t i);
    virtual std::vector<VkClearValue> getClearValue();

protected:
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkBuffer m_paramsBuffer;
    VkDeviceMemory m_paramsMemory;
    
    //color, imageview
    VkImage m_colorImage;
    VkDeviceMemory m_colorMemory;
    VkImageView m_colorImageView;
    
    //read.
    VkPipeline m_readPipeline;
    VkPipelineLayout m_readPipelineLayout;
    VkDescriptorSet m_readDescriptorSet;
    VkDescriptorSetLayout m_readDescriptorSetLayout;
    
private:
    GltfLoader m_gltfLoader;
};

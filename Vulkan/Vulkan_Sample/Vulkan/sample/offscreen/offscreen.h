
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class OffScreen : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    };
    
    OffScreen(std::string title);
    virtual ~OffScreen();
    
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
    virtual void createOtherBuffer();
//    virtual void createAttachmentDescription();
//    virtual void createRenderPass();

protected:
    //pass 1 mirror
    
    //pass 2 debug
    
    //pass 3 plane
    
    //pass 4 dragon
    
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
private:
    GltfLoader m_dragonLoader;
    GltfLoader m_planeLoader;
};

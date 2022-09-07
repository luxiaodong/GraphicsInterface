
#pragma once

#include "common/application.h"

class RenderHeadless : public Application
{
public:
    
    struct Vertex {
        float position[3];
        float color[3];
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };
    
    RenderHeadless(std::string title);
    virtual ~RenderHeadless();
    
    virtual void init();
    virtual void run();
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
    
    void createColorBuffer();
    void createFramebuffer();
    void renderToFile();

protected:
    VkFramebuffer m_framebuffer;
    
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;
    //顶点绑定和顶点描述
    std::vector<VkVertexInputBindingDescription> m_vertexInputBindDes;
    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttrDes;
    
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    VkFormat m_colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkImage m_colorImage;
    VkDeviceMemory m_colorMemory;
    VkImageView m_colorImageView;
};

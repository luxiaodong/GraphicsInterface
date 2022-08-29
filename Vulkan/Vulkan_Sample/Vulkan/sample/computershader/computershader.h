
#pragma once

#include "common/application.h"
#include "common/texture.h"

class ComputerShader : public Application
{
public:
    
    struct Vertex {
        float position[3];
        float uv[2];
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
    };
    
    ComputerShader(std::string title);
    virtual ~ComputerShader();
    
    virtual void init();
    virtual void initCamera();
    virtual void clear();
    virtual void render();
    
    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    
protected:
    void prepareVertex();
    void prepareUniform();
    void prepareTextureTarget();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();
    void createComputePipeline();

protected:
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
    
    Texture* m_pTexture;
    Texture* m_pComputerTarget;
    
    VkDescriptorSet m_preComputerDescriptorSet;
    VkDescriptorSet m_postComputerDescriptorSet;
    
    VkPipeline m_computerPipeline;
    VkCommandPool m_computerCommandPool;
    VkCommandBuffer m_computerCommandBuffer;
    VkPipelineLayout m_computerPipelineLayout;
    VkDescriptorSetLayout m_computerDescriptorSetLayout;
    VkDescriptorSet m_computerDescriptorSet;
    VkSemaphore m_computerSemaphore;
    VkSemaphore m_graphicsSemaphore;
};

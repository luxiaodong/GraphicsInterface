
#pragma once

#include "common/application.h"

#define OBJECT_INSTANCES 4

class DynamicUniformBuffer : public Application
{
public:
    
    struct Vertex {
        float position[3];
        float color[3];
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
//        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };

    struct UniformDynamic
    {
        unsigned char *pModelMatrix = nullptr;
    };
    
    DynamicUniformBuffer(std::string title);
    virtual ~DynamicUniformBuffer();
    
    virtual void init();
    virtual void initCamera();
    virtual void clear();
    
    virtual void prepareRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    
protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();
    
private:
    void dynamicAlignment();

protected:
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;
    //顶点绑定和顶点描述
    std::vector<VkVertexInputBindingDescription> m_vertexInputBindDes;
    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttrDes;
    
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    uint32_t m_indexCount;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkBuffer m_modelBuffer;
    VkDeviceMemory m_modelMemory;
    
//    UniformDynamic m_m;

    size_t m_matrixAlignment;
};

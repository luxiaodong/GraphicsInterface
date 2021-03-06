
#pragma once

#include "common/application.h"
#include "common/noise.h"

class Texture3Dim : public Application
{
public:
    
    struct Vertex {
        float position[3];
        float uv[2];
        float color[3];
    };
    
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 viewPos;
        float depth = 0.0f;
    };
    
    Texture3Dim(std::string title);
    virtual ~Texture3Dim();
    
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
    
private:
    Noise* m_pNoise;
};

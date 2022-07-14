
#pragma once

#include "common/application.h"
#include "common/texture.h"

class TextureArray : public Application
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
    
    struct InstanceData {
        // Model matrix
        glm::mat4 model;
        // Texture array index
        // Vec4 due to padding
        glm::vec4 arrayIndex;
    };
    
    TextureArray(std::string title);
    virtual ~TextureArray();
    
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
    uint32_t m_indexCount;
    
    InstanceData* m_pInstanceData = nullptr;
    Texture* m_pTexture = nullptr;
};

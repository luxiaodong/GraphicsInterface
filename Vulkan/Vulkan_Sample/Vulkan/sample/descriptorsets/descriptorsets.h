
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class Descriptorsets : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };
    
    struct Cube
    {
        Uniform matrix;
        VkBuffer m_uniformBuffer;
        VkDeviceMemory m_uniformMemory;
        VkDescriptorSet descriptorSet;
        glm::vec3 rotation;
        
//            vks::Texture2D texture;
    };

    Descriptorsets(std::string title);
    virtual ~Descriptorsets();

    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();

    virtual void prepareRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);

protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();

//protected:
//    VkPipelineLayout m_pipelineLayout;
//    VkPipeline m_graphicsPipeline;
//
//    //顶点绑定和顶点描述
//    std::vector<VkVertexInputBindingDescription> m_vertexInputBindDes;
//    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttrDes;
//    VkBuffer m_vertexBuffer;
//    VkDeviceMemory m_vertexMemory;
//    VkBuffer m_indexBuffer;
//    VkDeviceMemory m_indexMemory;
//
//    VkBuffer m_uniformBuffer;
//    VkDeviceMemory m_uniformMemory;
//    VkDescriptorSetLayout m_descriptorSetLayout;
//    VkDescriptorSet m_descriptorSet;
    
private:
    GltfLoader m_gltfLoader;
};

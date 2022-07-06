
#pragma once

#include "common/application.h"

class Pipelines : public Application
{
//public:
//    struct Vertex {
//        float position[3];
//        float color[3];
//    };
//
//    struct Uniform {
//        glm::mat4 projectionMatrix;
//        glm::mat4 modelMatrix;
//        glm::mat4 viewMatrix;
//    };
//
    Pipelines(std::string title);
    virtual ~Pipelines();

    virtual void init();
    virtual void clear();
//
//    virtual void prepareRenderData();
//    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
//
//protected:
//    void createDescriptorSetLayout();
//    void createPipelineLayout();
//    void createGraphicsPipeline();
//    void recordCommandBuffers();
//
//    void prepareVertex();
//    void prepareUniform();
//
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
};

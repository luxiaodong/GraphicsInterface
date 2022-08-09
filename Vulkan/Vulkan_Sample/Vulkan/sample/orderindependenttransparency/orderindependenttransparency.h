
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"

class OrderIndependentTransparency : public Application
{
public:
    struct Uniform
    {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
    };
    
    struct PushConstantData
    {
        glm::mat4 model;
        glm::vec4 color;
    };
    
    struct GeometryBuffer
    {
        uint32_t count;
        uint32_t maxNodeCount;
    };
    
    struct Node
    {
        glm::vec4 color;
        float depth;
        uint32_t next;
    };

    OrderIndependentTransparency(std::string title);
    virtual ~OrderIndependentTransparency();

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
    
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);
    virtual void createOtherBuffer();
    void createGeometryRenderPass();
    void createGeometryFrameBuffer();
    
protected:
    //geometry
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkBuffer m_geometryUniformBuffer;
    VkDeviceMemory m_geometryUniformMemory;
    VkBuffer m_nodeUniformBuffer;
    VkDeviceMemory m_nodeUniformMemory;
    
    VkPushConstantRange m_pushConstantRange;
    VkDescriptorSetLayout m_geometryDescriptorSetLayout;
    VkPipelineLayout m_geometryPipelineLayout;
    VkDescriptorSet m_geometryDescriptorSet;
    VkPipeline m_geometryPipeline;
    VkRenderPass m_geometryRenderPass;
    VkFramebuffer m_geometryFrameBuffer;
    VkImage         m_geometryImage;
    VkDeviceMemory  m_geometryMemory;
    VkImageView     m_geometryImageView;
    VkFormat m_geometryFormat = VK_FORMAT_R32_UINT;
    
    //color == appliction
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_pipeline;
    
private:
    GltfLoader m_cubeLoader;
    GltfLoader m_sphereLoader;
};


#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class OcclusionQuery : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 color = glm::vec4(0.0f);
        glm::vec4 lightPos = glm::vec4(10.0f, -10.0f, 10.0f, 1.0f);
        float visible;
    };

    OcclusionQuery(std::string title);
    virtual ~OcclusionQuery();

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
    
private:
    VkBuffer m_sphereBuffer;
    VkDeviceMemory m_sphereMemory;
    VkBuffer m_teapotBuffer;
    VkDeviceMemory m_teapotMemory;
    VkBuffer m_occluderBuffer;
    VkDeviceMemory m_occluderMemory;
    
    VkPipeline m_simplePipeline;
    VkPipeline m_solidPipeline;
    VkPipeline m_occluderPipeline;
    
    VkDescriptorSet m_sphereDescriptorSet;
    VkDescriptorSet m_teapotDescriptorSet;
    VkDescriptorSet m_descriptorSet;
    
private:
    GltfLoader m_sphereLoader;
    GltfLoader m_teapotLoader;
    GltfLoader m_planeLoader;
};

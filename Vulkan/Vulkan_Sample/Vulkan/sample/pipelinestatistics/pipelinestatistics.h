
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class PipelineStatistics : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos = glm::vec4(-10.0f, -10.0f, 10.0f, 1.0f);
    };

    PipelineStatistics(std::string title);
    virtual ~PipelineStatistics();

    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();

    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    virtual void queueResult();
    virtual void createOtherRenderPass(const VkCommandBuffer& commandBuffer);

protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();
    
    void prepareQueryPool();
    
private:
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkPipeline m_pipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkQueryPool m_queryPool;
    bool m_blending = false;
    bool m_discard = false;
    bool m_wireframe = false;
    bool m_tessellation = false;
    
    std::vector<uint64_t> m_pipelineStatValues;
    std::vector<std::string> m_pipelineStatNames;

private:
    GltfLoader m_gltfLoader;
};

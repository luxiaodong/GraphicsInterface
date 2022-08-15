
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

#define INSTANCE_COUNT 4096

class Instancing : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, -5.0f, 0.0f, 1.0f);
        float locSpeed = 0.0f;
        float globSpeed = 0.0f;
    };
    
    struct InstanceData {
        glm::vec3 pos;
        glm::vec3 rot;
        float scale;
        uint32_t texIndex;
    };

    Instancing(std::string title);
    virtual ~Instancing();

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
    void prepareInstanceData();
    
private:
    // background
    VkPipeline m_bgPipeline;
    
    // planet
    VkPipeline m_pipeline;
    VkDescriptorSet m_descriptorSet;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    // instanced rock
    VkPipeline m_instanceRockPipeline;
    VkDescriptorSet m_instanceRockdescriptorSet;
    VkBuffer m_instanceBuffer;
    VkDeviceMemory m_instanceMemory;

private:
    GltfLoader m_planetLoader;
    GltfLoader m_rocksLoader;
    Texture* m_pPlanet;
    Texture* m_pRocks;
};


#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class IndirectDraw : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
    };
    
    struct InstanceData {
        glm::vec3 pos;
        glm::vec3 rot;
        float scale;
        uint32_t texIndex;
    };

    IndirectDraw(std::string title);
    virtual ~IndirectDraw();

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
    
    void prepareIndirectData();
    void prepareInstanceData();
    
private:
    // sky
    VkPipeline m_skyPipeline;
    
    // ground
    VkPipeline m_pipeline;
    VkDescriptorSet m_descriptorSet;
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    // plants
    VkPipeline m_plantsPipeline;
    VkDescriptorSet m_plantsDescriptorSet;
    VkBuffer m_instanceBuffer;
    VkDeviceMemory m_instanceMemory;
    VkBuffer m_indirectBuffer;
    VkDeviceMemory m_indirectMemory;
    
    uint32_t m_objectCount = 0;
    // Store the indirect draw commands containing index offsets and instance count per object
    std::vector<VkDrawIndexedIndirectCommand> m_indirectCommands;
    
private:
    GltfLoader m_skysphereLoader;
    GltfLoader m_groundLoader;
    GltfLoader m_plantsLoader;

    Texture* m_pPlants;
    Texture* m_pGround;
};

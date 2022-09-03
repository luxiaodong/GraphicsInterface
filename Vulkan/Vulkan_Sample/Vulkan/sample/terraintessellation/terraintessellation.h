
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class TerrainTessellation : public Application
{
public:
    struct SkyboxUniform
    {
        glm::mat4 mvp;
    };
    
    struct TessControl
    {
        float tessLevel = 64.0f;
    };
    
    struct TessEval
    {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::vec4 lightPos = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        float tessAlpha = 1.0f;
        float tessStrength = 0.1f;
    };
    
    TerrainTessellation(std::string title);
    virtual ~TerrainTessellation();
    
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

protected:
    // skybox
    VkPipeline m_skyboxPipeline;
    VkPipelineLayout m_skyboxPipelineLayout;
    VkDescriptorSetLayout m_skyboxDescriptorSetLayout;
    VkDescriptorSet m_skyboxDescriptorSet;
    VkBuffer m_skyboxUniformBuffer;
    VkDeviceMemory m_skyboxUniformMemory;
    
    // terrain
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_tessEvalmBuffer;
    VkDeviceMemory m_tessEvalMemory;
    VkBuffer m_tessControlmBuffer;
    VkDeviceMemory m_tessControlMemory;
    
private:
    GltfLoader m_skyboxLoader;
    
    Texture* m_pSkyboxMap; //天空盒
    Texture* m_pTerrainMap;//地形贴图
    Texture* m_pHeightMap; //高度贴图
};

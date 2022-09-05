
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"
#include "common/frustum.h"

class TerrainHeight
{
public:
    TerrainHeight(std::string filename, uint32_t patchsize);
    ~TerrainHeight();
    
    float getHeight(uint32_t x, uint32_t y);
    
private:
    uint16_t* m_pData;
    uint32_t  m_length;
    uint32_t  m_tileCount;
};

class TerrainTessellation : public Application
{
public:
    struct TerrainVertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
    };
    
    struct SkyboxUniform
    {
        glm::mat4 mvp;
    };
    
    struct TessEval
    {
        glm::mat4 projection;
        glm::mat4 modelview;
        glm::vec4 lightPos = glm::vec4(-48.0f, -40.0f, 46.0f, 0.0f);
        glm::vec4 frustumPlanes[6];
        float displacementFactor = 32.0f;
        float tessellationFactor = 0.75f;
        glm::vec2 viewportDim;
        float tessellatedEdgeSize = 20.0f;
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
    
    void createTerrain();

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
    
private:
    //顶点绑定和顶点描述
    std::vector<VkVertexInputBindingDescription> m_vertexInputBindDes;
    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttrDes;
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    uint32_t m_terrainIndexCount;
    
private:
    GltfLoader m_skyboxLoader;
    
    Texture* m_pSkyboxMap; //天空盒
    Texture* m_pTerrainMap;//地形贴图
    Texture* m_pHeightMap; //高度贴图
};

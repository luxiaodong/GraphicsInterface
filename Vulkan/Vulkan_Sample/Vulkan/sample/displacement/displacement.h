
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class Displacement : public Application
{
public:
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
    
    Displacement(std::string title);
    virtual ~Displacement();
    
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
    VkPipeline m_wireframePipeline;
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_tessEvalmBuffer;
    VkDeviceMemory m_tessEvalMemory;
    VkBuffer m_tessControlmBuffer;
    VkDeviceMemory m_tessControlMemory;
    
private:
    GltfLoader m_objectLoader;
    Texture* m_pHeightMap; //高度贴图
};

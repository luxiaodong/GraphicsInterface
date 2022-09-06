
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class CurvedPnTriangles : public Application
{
public:
    struct TessControl
    {
        float tessLevel = 3.0f;
    };
    
    struct TessEval
    {
        glm::mat4 projection;
        glm::mat4 modelView;
        float tessAlpha = 1.0f;
    };
    
    CurvedPnTriangles(std::string title);
    virtual ~CurvedPnTriangles();
    
    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();
    
    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    virtual std::vector<VkClearValue> getClearValue();
    
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
};

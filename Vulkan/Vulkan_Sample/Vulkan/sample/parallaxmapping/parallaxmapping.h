
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class ParallaxMapping : public Application
{
public:
    struct UniformVert {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 lightPos = glm::vec4(0.0f, -2.0f, 0.0f, 1.0f);
        glm::vec4 cameraPos;
    };
    
    struct UniformFrag {
        float heightScale = 0.1f;
        float parallaxBias = -0.02f;
        float numLayers = 48.0f;
        int32_t mappingMode = 4;
    };
    
    ParallaxMapping(std::string title);
    virtual ~ParallaxMapping();
    
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
    VkPipeline m_graphicsPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_uniformVertBuffer;
    VkDeviceMemory m_uniformVertMemory;
    VkBuffer m_uniformFragBuffer;
    VkDeviceMemory m_uniformFragMemory;
private:
    GltfLoader m_planeLoader;
    Texture* m_pColor;
    Texture* m_pNormal; // height = alpha channel
};

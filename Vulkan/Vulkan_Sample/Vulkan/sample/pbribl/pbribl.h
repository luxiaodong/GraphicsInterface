
#pragma once

#include "common/application.h"
#include "common/gltfLoader.h"

class PbrIbl : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        glm::vec3 camPos;
    };
    
    struct LightParams
    {
        glm::vec4 lights[4];
        float exposure = 4.5f;
        float gamma = 2.2f;
    };
    
    PbrIbl(std::string title);
    virtual ~PbrIbl();

    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();
    virtual void betweenInitAndLoop();

    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);

protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();
    
protected:
    void selectPbrMaterial();
    void createIrrImage();
    void generateBrdfLut();
    void generateIrradianceCube();
    void generatePrefilteredCube();

private:
    // lut
    VkImage m_lutImage;
    VkDeviceMemory m_lutMemory;
    VkImageView m_lutImageView;
    VkSampler m_lutSampler;
    
    // irr
    VkImage m_irrImage;
    VkDeviceMemory m_irrMemory;
    VkImageView m_irrImageView;
    VkSampler m_irrSampler;
    uint32_t m_irrMaxLevels;
    
    // filter
    VkImage m_filterImage;
    VkDeviceMemory m_filterMemory;
    VkImageView m_filterImageView;
    VkSampler m_filterSampler;
    
    // skybox
    VkDescriptorSet m_skyboxDescriptorSet;
    VkDescriptorSetLayout m_skyboxDescriptorSetLayout;
    VkPipelineLayout m_skyboxPipelineLayout;
    VkPipeline m_skyboxPipeline;
    
    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    VkBuffer m_lightBuffer;
    VkDeviceMemory m_lightMemory;
    
    VkPipeline m_pipeline;
    VkDescriptorSet m_descriptorSet;

    PbrMaterial m_pbrMaterial;
    
private:
    GltfLoader m_gltfLoader;
    GltfLoader m_skyboxLoader;
    
    Texture* m_pEnvCube;
};

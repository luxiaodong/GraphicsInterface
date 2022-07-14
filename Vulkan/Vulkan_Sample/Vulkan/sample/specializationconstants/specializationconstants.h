
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"

class SpecializationConstants : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
//        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        glm::vec4 lightPos;
    };
    
    struct SpecializationData {
        // Sets the lighting model used in the fragment "uber" shader
        uint32_t lightingModel;
        // Parameter for the toon shading part of the fragment shader
        float toonDesaturationFactor = 0.5f;
    };

    SpecializationConstants(std::string title);
    virtual ~SpecializationConstants();

    virtual void init();
    virtual void initCamera();
//    virtual void setEnabledFeatures();
    virtual void clear();

    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);

protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void prepareSpecializationInfo();
    void createGraphicsPipeline();

protected:
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_phong;
    VkPipeline m_toon;
    VkPipeline m_textured;

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    SpecializationData m_specializationData = {};
    std::array<VkSpecializationMapEntry, 2> m_specializationMapEntries;
    VkSpecializationInfo m_specializationInfo = {};
    Texture* m_colorMap = nullptr;

private:
    GltfLoader m_gltfLoader;
};

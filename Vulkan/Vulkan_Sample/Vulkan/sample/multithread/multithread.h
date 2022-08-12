
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"
#include "common/thread.h"
#include "common/frustum.h"

class MultiThread : public Application
{
public:
    struct Uniform
    {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
    };
    
    struct PushConstantBlock
    {
        glm::mat4 mvp;
        glm::vec3 color;
    };
    
    struct ObjectData {
        glm::mat4 model;
        glm::vec3 pos;
        glm::vec3 rotation;
        float rotationDir;
        float rotationSpeed;
        float scale;
        float deltaT;
        float stateT = 0;
        bool visible = true;
    };
    
    struct ThreadData {
        VkCommandPool commandPool;
        // One command buffer per render object
        std::vector<VkCommandBuffer> commandBuffer;
        // One push constant block per render object
        std::vector<PushConstantBlock> pushConstBlock;
        // Per object information (position, rotation, etc.)
        std::vector<ObjectData> objectData;
    };
    
    MultiThread(std::string title);
    virtual ~MultiThread();

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

    void createSecondaryCommandBuffer();
    void prepareMultiThread();
    void updateSecondaryCommandBuffers(VkCommandBufferInheritanceInfo inheritanceInfo);
    void threadRenderCode(uint32_t threadIndex, uint32_t cmdBufferIndex, VkCommandBufferInheritanceInfo inheritanceInfo);
    
protected:
    uint32_t m_threadCount;
    uint32_t m_objectCountPerThread;
    std::vector<ThreadData*> m_threadDatas;
    ThreadPool m_threadPool;
    VkFence m_renderFence;
    
    // m_commandBuffer; 当成主commandBuffer
    VkCommandBuffer m_secondaryCommandBuffer;
    
    Frustum m_frustum;
    
    // ufo
    VkPushConstantRange m_ufoPushConstantRange;
    VkPipeline m_ufoPipeline;
    
    // sphere
    VkPipeline m_pipeline;
    VkDescriptorSet m_descriptorSet;
    
private:
    GltfLoader m_ufoLoader;
    GltfLoader m_sphereLoader;
};

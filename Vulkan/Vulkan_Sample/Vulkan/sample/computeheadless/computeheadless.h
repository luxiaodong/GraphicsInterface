
#pragma once

#include "common/application.h"

class ComputeHeadless : public Application
{
public:
    ComputeHeadless(std::string title);
    virtual ~ComputeHeadless();
    
    virtual void init();
    virtual void run();
    virtual void clear();
    
    void prepareStorageBuffers();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createComputerPipelines();
    void createRenderCommand();
    
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer){}

protected:
    VkPipeline m_computerPipeline;
    VkDescriptorSet m_descriptorSet;
    
    VkBuffer m_hostBuffer;
    VkDeviceMemory m_hostMemory;
    VkBuffer m_deviceBuffer;
    VkDeviceMemory m_deviceMemory;
    
    std::vector<uint32_t> m_inData;
    std::vector<uint32_t> m_outData;
};

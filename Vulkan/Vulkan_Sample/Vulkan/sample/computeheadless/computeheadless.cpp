
#include "computeheadless.h"

#define BUFFER_ELEMENTS 32

ComputeHeadless::ComputeHeadless(std::string title) : Application(title)
{
}

ComputeHeadless::~ComputeHeadless()
{}

void ComputeHeadless::run()
{
    init();
    clear();
}

void ComputeHeadless::init()
{
//    Application::init();
    
    Tools::init();
    createInstance();
    choosePhysicalDevice();
    Tools::m_physicalDevice = m_physicalDevice;
    
    m_familyIndices.graphicsFamily = 0;
    m_familyIndices.computerFamily = 0;
    m_familyIndices.presentFamily = 0;
    
    createLogicDeivce();
    Tools::m_device = m_device;
    Tools::m_deviceEnabledFeatures = m_deviceEnabledFeatures;
    Tools::m_deviceProperties = m_deviceProperties;
    Tools::m_graphicsQueue = m_graphicsQueue;
    Tools::m_computerQueue = m_computerQueue;
    
    createCommandPool();
    Tools::m_commandPool = m_commandPool;
    
    m_swapchainExtent.width = m_width * 2;
    m_swapchainExtent.height = m_height * 2;
    
    createPipelineCache();

    prepareStorageBuffers();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createComputerPipelines();
    createRenderCommand();
    
    vkDeviceWaitIdle(m_device);
    
    // Output buffer contents
    printf("Compute input:\n");
    for (auto v : m_inData) {
        printf("%d \t", v);
    }
    std::cout << std::endl;

    printf("Compute output:\n");
    for (auto v : m_outData) {
        printf("%d \t", v);
    }
    std::cout << std::endl;
}

void ComputeHeadless::clear()
{
    vkDestroyPipeline(m_device, m_computerPipeline, nullptr);
    vkFreeMemory(m_device, m_hostMemory, nullptr);
    vkDestroyBuffer(m_device, m_hostBuffer, nullptr);
    vkFreeMemory(m_device, m_deviceMemory, nullptr);
    vkDestroyBuffer(m_device, m_deviceBuffer, nullptr);
    
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void ComputeHeadless::prepareStorageBuffers()
{
    m_inData.resize(BUFFER_ELEMENTS);
    m_outData.resize(BUFFER_ELEMENTS);
    
    uint32_t n = 0;
    std::generate(m_inData.begin(), m_inData.end(), [&n] { return n++; });
    
    VkDeviceSize bufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
    
    Tools::createBufferAndMemoryThenBind(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_hostBuffer, m_hostMemory);
    
    Tools::mapMemory(m_hostMemory, bufferSize, m_inData.data());
    
    Tools::createBufferAndMemoryThenBind(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_deviceBuffer, m_deviceMemory);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy bufferCopy = {};
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = 0;
    bufferCopy.size = bufferSize;
    vkCmdCopyBuffer(cmd, m_hostBuffer, m_deviceBuffer, 1, &bufferCopy);
    
    Tools::flushCommandBuffer(cmd, m_computerQueue, true);
}

void ComputeHeadless::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    createDescriptorSetLayout(&binding, 1);
    createPipelineLayout();
}

void ComputeHeadless::prepareDescriptorSetAndWrite()
{
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;
    
    createDescriptorPool(&poolSize, 1, 1);
    createDescriptorSet(m_descriptorSet);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = BUFFER_ELEMENTS * sizeof(uint32_t);
    bufferInfo.buffer = m_deviceBuffer;
    
    VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &bufferInfo);
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void ComputeHeadless::createComputerPipelines()
{
    struct SpecializationData
    {
        uint32_t BUFFER_ELEMENT_COUNT = BUFFER_ELEMENTS;
    } specializationData;
    
    VkSpecializationMapEntry specializationMapEntry;
    specializationMapEntry.constantID = 0;
    specializationMapEntry.size = sizeof(int);
    specializationMapEntry.offset = 0;
    
    VkSpecializationInfo specializationInfo = {};
    specializationInfo.dataSize = sizeof(int);
    specializationInfo.mapEntryCount = 1;
    specializationInfo.pMapEntries = &specializationMapEntry;
    specializationInfo.pData = &specializationData;

    VkShaderModule compModule = Tools::createShaderModule(Tools::getShaderPath() + "computeheadless/headless.comp.spv");
    VkPipelineShaderStageCreateInfo stageInfo = Tools::getPipelineShaderStageCreateInfo(compModule, VK_SHADER_STAGE_COMPUTE_BIT);
    stageInfo.pSpecializationInfo = &specializationInfo;
    
    VkComputePipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage = stageInfo;
    createInfo.layout = m_pipelineLayout;
    VK_CHECK_RESULT(vkCreateComputePipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_computerPipeline));
    vkDestroyShaderModule(m_device, compModule, nullptr);
}

void ComputeHeadless::createRenderCommand()
{
    VkCommandBuffer commandBuffer = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computerPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, 0);
    vkCmdDispatch(commandBuffer, BUFFER_ELEMENTS, 1, 1);
    Tools::flushCommandBuffer(commandBuffer, m_computerQueue, true);

    VkDeviceSize bufferSize = BUFFER_ELEMENTS * sizeof(uint32_t);
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion = {};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, m_deviceBuffer, m_hostBuffer, 1, &copyRegion);
    Tools::flushCommandBuffer(cmd, m_computerQueue, true);
    
    void* mapped;
    vkMapMemory(m_device, m_hostMemory, 0, VK_WHOLE_SIZE, 0, &mapped);
    VkMappedMemoryRange mappedRange {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = m_hostMemory;
    mappedRange.offset = 0;
    mappedRange.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(m_device, 1, &mappedRange);
    memcpy( m_outData.data(), mapped, bufferSize);
}

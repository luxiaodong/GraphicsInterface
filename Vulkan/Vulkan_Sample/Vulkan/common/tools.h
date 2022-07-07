
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <optional>
#include <cstdint>
#include <fstream>
#include <array>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
//
//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>
//
//#define TINYOBJLOADER_IMPLEMENTATION
//#include <tiny_obj_loader.h>

#define VK_CHECK_RESULT(f)                      \
{                                               \
    VkResult res = (f);                         \
    if (res != VK_SUCCESS)                      \
    {                                           \
        std::cout << "Fatal : VkResult is \"" << res << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
        assert(res == VK_SUCCESS);              \
    }                                           \
}

class Tools
{
public:
    static std::string getShaderPath();
    static std::string getModelPath();
    static bool isFileExists(const std::string &filename);
    static std::vector<char> readFile(const std::string& filename);
    
    static VkPhysicalDevice m_physicalDevice;
    static VkDevice m_device;
    static VkCommandPool m_commandPool;
    
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static void createBufferAndMemoryThenBind(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, VkBuffer &buffer, VkDeviceMemory& memory);
    static void createImageAndMemoryThenBind(VkFormat format, uint32_t width, uint32_t height, uint32_t lodLevels, VkSampleCountFlagBits sampleFlag, VkImageUsageFlags usage, VkImageTiling tiling, VkMemoryPropertyFlags flags, VkImage &image, VkDeviceMemory &imageMemory);
    static void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t LodLevel, VkImageView &imageView);
    static void mapMemory(VkDeviceMemory &memory, VkDeviceSize size, void* srcAddress);
    static VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin, uint32_t count = 1);
    static void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);
    static void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange);
    static void setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageAspectFlags aspectMask);
    static void createTextureSampler(VkFilter filter, VkSamplerAddressMode addressMode, uint32_t maxLod, VkSampler &sampler);

    static VkDescriptorSetLayoutCreateInfo getDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding* pBindings, uint32_t bindingCount);
    static void allocateDescriptorSets(VkDescriptorPool descriptorPool, VkDescriptorSetLayout* pSetLayouts, uint32_t descriptorSetCount, VkDescriptorSet& descriptorSet);
    static VkDescriptorSetLayoutBinding getDescriptorSetLayoutBinding(VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t count = 1);
    static VkWriteDescriptorSet getWriteDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType descriptorType, uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count = 1);
    static VkWriteDescriptorSet getWriteDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType descriptorType, uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count = 1);

    static VkShaderModule createShaderModule(const std::string& filename);
    static VkPipelineShaderStageCreateInfo getPipelineShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits stage);
    
    static VkVertexInputBindingDescription getVertexInputBindingDescription(uint32_t binding, uint32_t stride);
    static VkVertexInputAttributeDescription getVertexInputAttributeDescription(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
    
    static VkPipelineVertexInputStateCreateInfo getPipelineVertexInputStateCreateInfo(std::vector<VkVertexInputBindingDescription> &bindingDescriptions, std::vector<VkVertexInputAttributeDescription> &attributeDescriptions);
    static VkPipelineInputAssemblyStateCreateInfo getPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable);
    static VkViewport getViewport(float x, float y, float width, float height);
    static VkPipelineViewportStateCreateInfo getPipelineViewportStateCreateInfo( std::vector<VkViewport>& viewPorts, std::vector<VkRect2D>& scissors);
    static VkPipelineRasterizationStateCreateInfo getPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlagBits cullMode, VkFrontFace frontFace);
    static VkPipelineMultisampleStateCreateInfo getPipelineMultisampleStateCreateInfo(VkSampleCountFlagBits sampleCount);
    static VkPipelineDepthStencilStateCreateInfo getPipelineDepthStencilStateCreateInfo(VkBool32 depthTest, VkBool32 depthWrite, VkCompareOp compareOp);
    static VkPipelineColorBlendAttachmentState getPipelineColorBlendAttachmentState(VkBool32 blend, VkColorComponentFlags colorComponent);
    static VkPipelineColorBlendStateCreateInfo getPipelineColorBlendStateCreateInfo(uint32_t count, VkPipelineColorBlendAttachmentState* pColorBlendAttachment);
    static VkPipelineColorBlendStateCreateInfo getPipelineColorBlendStateCreateInfo(std::vector<VkPipelineColorBlendAttachmentState>& colorBlendAttachment);
    
    static VkGraphicsPipelineCreateInfo getGraphicsPipelineCreateInfo(VkPipelineLayout layout, VkRenderPass renderPass);
    static VkGraphicsPipelineCreateInfo getGraphicsPipelineCreateInfo(std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, VkPipelineLayout layout, VkRenderPass renderPass);
    static VkPipelineDynamicStateCreateInfo getPipelineDynamicStateCreateInfo(std::vector<VkDynamicState>& dynamicStates);
    static VkPipelineDynamicStateCreateInfo getPipelineDynamicStateCreateInfo(uint32_t count, VkDynamicState* pDynamicState);
};


#pragma once

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

#include <vulkan/vulkan.h>

class Tools
{
public:
    static VkPhysicalDevice m_physicalDevice;
    static VkDevice m_device;
    static VkCommandPool m_commandPool;
    
    static std::vector<char> readFile(const std::string& filename);
    static VkShaderModule createShaderModule(const std::string& filename);
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
};

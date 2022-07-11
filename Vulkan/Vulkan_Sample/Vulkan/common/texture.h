
#pragma once

#include "tools.h"

#include <ktx.h>
#include <ktxvulkan.h>

class Texture
{
public:
    Texture();
    ~Texture();
    
public:
    static Texture* loadTextrue2D(std::string fileName, VkQueue transferQueue, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
    
public:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevels;
    uint32_t m_layerCount;
    
    VkImage         m_image;
    VkImageView     m_imageView;
    VkDeviceMemory  m_imageMemory;
    VkImageLayout   m_imageLayout;
    VkDescriptorImageInfo   m_descriptorImageInfo;
    VkSampler       m_sampler;
};

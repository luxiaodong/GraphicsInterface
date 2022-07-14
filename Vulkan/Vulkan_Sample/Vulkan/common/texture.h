
#pragma once

#include "tools.h"

#include <ktx.h>
#include <ktxvulkan.h>

enum TextureCopyRegion { Nothing, MipLevel, Layer};

class Texture
{
public:
    Texture();
    ~Texture();
    
public:
    static Texture* loadTextrue2D(std::string fileName, VkQueue transferQueue, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion copyRegion = TextureCopyRegion::MipLevel, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    void clear();
    VkDescriptorImageInfo getDescriptorImageInfo();
    
public:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevels;
    uint32_t m_layerCount;

    VkImageLayout   m_imageLayout;
    VkFormat        m_fromat;
    VkImage         m_image;
    VkDeviceMemory  m_imageMemory;
    VkImageView     m_imageView;
    VkSampler       m_sampler;
};

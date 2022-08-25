
#pragma once

#include "tools.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include "tiny_gltf.h"

enum TextureCopyRegion { Nothing, MipLevel, Layer, Cube, CubeArry};

class Texture
{
public:
    Texture();
    ~Texture();
    
public:
    static Texture* loadTextrue2D(std::string fileName, VkQueue transferQueue, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion copyRegion = TextureCopyRegion::MipLevel, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    static Texture* loadTexture2D(tinygltf::Image &gltfimage, std::string path, VkQueue transferQueue);
    static Texture* loadTextureEmpty(VkQueue transferQueue);
    
    static Texture* loadTextrue2D(void* buffer, VkDeviceSize bufferSize, uint32_t width, uint32_t height, VkFormat format, VkQueue transferQueue);
    
    static void fillTextrue(Texture* texture, ktxTexture* ktxTexture, VkQueue transferQueue, TextureCopyRegion copyRegion);
    static void fillTextrue(Texture* texture, void* buffer, VkDeviceSize bufferSize, VkQueue transferQueue);
    
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
    
    //在主逻辑赋值.
    VkDescriptorSet m_descriptorSet;
    std::string m_name = "";
};

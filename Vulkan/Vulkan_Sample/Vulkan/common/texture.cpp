
#include "texture.h"

Texture::Texture()
{
}

Texture::~Texture()
{}

VkDescriptorImageInfo Texture::getDescriptorImageInfo()
{
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = m_imageLayout;
    imageInfo.imageView = m_imageView;
    imageInfo.sampler = m_sampler;
    return imageInfo;
}

void Texture::clear()
{
    vkDestroySampler(Tools::m_device, m_sampler, nullptr);
    vkDestroyImageView(Tools::m_device, m_imageView, nullptr);
    vkDestroyImage(Tools::m_device, m_image, nullptr);
    vkFreeMemory(Tools::m_device, m_imageMemory, nullptr);
}

Texture* Texture::loadTextrue2D(std::string fileName, VkQueue transferQueue, VkFormat format, VkImageLayout imageLayout)
{
    Texture* newTexture = new Texture();
    assert(Tools::isFileExists(fileName));
    ktxTexture* ktxTexture;
    ktxResult result = ktxTexture_CreateFromNamedFile(fileName.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
    assert(result == KTX_SUCCESS);

    newTexture->m_fromat = format;
    newTexture->m_imageLayout = imageLayout;
    newTexture->m_width = ktxTexture->baseWidth;
    newTexture->m_height = ktxTexture->baseHeight;
    newTexture->m_mipLevels = ktxTexture->numLevels;
    newTexture->m_mipLevels = 1;
    newTexture->m_layerCount = ktxTexture->numLayers;
    ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
    
    // Get device properties for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(Tools::m_physicalDevice, format, &formatProperties);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    Tools::createBufferAndMemoryThenBind(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stagingBuffer, stagingMemory);
    Tools::mapMemory(stagingMemory, ktxTextureSize, ktxTextureData);
    
    Tools::createImageAndMemoryThenBind(format, newTexture->m_width, newTexture->m_height, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        newTexture->m_image, newTexture->m_imageMemory);
    
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    for (uint32_t i = 0; i < newTexture->m_mipLevels; i++)
    {
        ktx_size_t offset;
        KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
        assert(result == KTX_SUCCESS);
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = i;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
        bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = offset;
        bufferCopyRegions.push_back(bufferCopyRegion);
    }
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = newTexture->m_mipLevels;
    subresourceRange.layerCount = 1;
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    Tools::setImageLayout(cmd, newTexture->m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, subresourceRange);

    vkCmdCopyBufferToImage(cmd, stagingBuffer, newTexture->m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

    Tools::setImageLayout(cmd, newTexture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    Tools::flushCommandBuffer(cmd, transferQueue, true);
    
    ktxTexture_Destroy(ktxTexture);
    vkFreeMemory(Tools::m_device, stagingMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, stagingBuffer, nullptr);
    
    Tools::createImageView(newTexture->m_image, format, VK_IMAGE_ASPECT_COLOR_BIT, newTexture->m_mipLevels, newTexture->m_imageView);
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, newTexture->m_mipLevels, newTexture->m_sampler);
    return newTexture;
}


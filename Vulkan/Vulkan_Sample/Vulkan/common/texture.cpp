
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

Texture* Texture::loadTextureEmpty(VkQueue transferQueue)
{
    Texture* newTexture = new Texture();
    newTexture->m_fromat = VK_FORMAT_R8G8B8A8_UNORM;
    newTexture->m_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    newTexture->m_width = 1;
    newTexture->m_height = 1;
    newTexture->m_layerCount = 1;
    newTexture->m_mipLevels = 1;
    VkDeviceSize bufferSize = newTexture->m_width * newTexture->m_height *  4;
    unsigned char* buffer = new unsigned char[bufferSize];
    memset(buffer, 0, bufferSize);
    fillTextrue(newTexture, buffer, bufferSize, transferQueue);
    delete[] buffer;
    return newTexture;
}

Texture* Texture::loadTexture2D(tinygltf::Image &gltfimage, std::string path, VkQueue transferQueue)
{
    bool isKtx = false;
    if (gltfimage.uri.find_last_of(".") != std::string::npos)
    {
        if (gltfimage.uri.substr(gltfimage.uri.find_last_of(".") + 1) == "ktx")
        {
            isKtx = true;
        }
    }
    
    if (!isKtx)
    {
        Texture* newTexture = new Texture();
        newTexture->m_fromat = VK_FORMAT_R8G8B8A8_UNORM;
        newTexture->m_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        newTexture->m_width = gltfimage.width;
        newTexture->m_height = gltfimage.height;
        newTexture->m_layerCount = 1;
        newTexture->m_mipLevels = static_cast<uint32_t>(floor(log2(std::max(newTexture->m_width, newTexture->m_height))) + 1.0);
        newTexture->m_name = gltfimage.uri;

        unsigned char* buffer = nullptr;
        VkDeviceSize bufferSize = 0;
        bool deleteBuffer = false;

        if (gltfimage.component == 3)
        {
            // Most devices don't support RGB only on Vulkan so convert if necessary
            bufferSize = gltfimage.width * gltfimage.height * 4;
            buffer = new unsigned char[bufferSize];
            unsigned char* rgba = buffer;
            unsigned char* rgb = &gltfimage.image[0];
            for (size_t i = 0; i < gltfimage.width * gltfimage.height; ++i) {
                for (int32_t j = 0; j < 3; ++j) {
                    rgba[j] = rgb[j];
                }
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else {
            buffer = &gltfimage.image[0];
            bufferSize = gltfimage.image.size();
        }
        
        fillTextrue(newTexture, buffer, bufferSize, transferQueue);
        
        if(deleteBuffer)
        {
            delete[] buffer;
        }
        
        return newTexture;
    }
    
    std::string fileName = path + "/" + gltfimage.uri;
    return loadTextrue2D(fileName, transferQueue);
}

void Texture::fillTextrue(Texture* texture, unsigned char* buffer, VkDeviceSize bufferSize, VkQueue transferQueue)
{
    // Get device properties for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(Tools::m_physicalDevice, texture->m_fromat, &formatProperties);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    Tools::createBufferAndMemoryThenBind(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stagingBuffer, stagingMemory);
    Tools::mapMemory(stagingMemory, bufferSize, buffer);
    Tools::createImageAndMemoryThenBind(texture->m_fromat, texture->m_width, texture->m_height, texture->m_mipLevels, texture->m_layerCount,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        texture->m_image, texture->m_imageMemory);
    
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    {
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = texture->m_width;
        bufferCopyRegion.imageExtent.height = texture->m_height;
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegions.push_back(bufferCopyRegion);
    }
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = texture->m_mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = texture->m_layerCount;
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Tools::setImageLayout(cmd, texture->m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

    vkCmdCopyBufferToImage(cmd, stagingBuffer, texture->m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

    Tools::setImageLayout(cmd, texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

    // Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
    if( texture->m_mipLevels > 1)
    {
        for (uint32_t i = 1; i < texture->m_mipLevels; i++)
        {
            VkImageBlit imageBlit{};
            imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.srcSubresource.layerCount = 1;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = int32_t(texture->m_width >> (i - 1));
            imageBlit.srcOffsets[1].y = int32_t(texture->m_height >> (i - 1));
            imageBlit.srcOffsets[1].z = 1;
            
            imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.dstSubresource.layerCount = 1;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = int32_t(texture->m_width >> i);
            imageBlit.dstOffsets[1].y = int32_t(texture->m_height >> i);
            imageBlit.dstOffsets[1].z = 1;
        
            VkImageSubresourceRange mipSubRange = {};
            mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipSubRange.baseMipLevel = i;
            mipSubRange.levelCount = 1;
            mipSubRange.layerCount = 1;
        
            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = 0;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.image = texture->m_image;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }

            vkCmdBlitImage(cmd, texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
                    
            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageMemoryBarrier.image = texture->m_image;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }
        }

        subresourceRange.levelCount = texture->m_mipLevels;

        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.image = texture->m_image;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
    }
    
    Tools::flushCommandBuffer(cmd, transferQueue, true);
    vkFreeMemory(Tools::m_device, stagingMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, stagingBuffer, nullptr);
    
    Tools::createImageView(texture->m_image, texture->m_fromat, VK_IMAGE_ASPECT_COLOR_BIT, texture->m_mipLevels, texture->m_layerCount, texture->m_imageView);
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, texture->m_mipLevels, texture->m_sampler);
}

void Texture::fillTextrue(Texture* texture, ktxTexture* ktxTexture, VkQueue transferQueue, TextureCopyRegion copyRegion)
{
//    Texture* newTexture = new Texture();
//    assert(Tools::isFileExists(fileName));
//    ktxTexture* ktxTexture;
//    ktxResult result = ktxTexture_CreateFromNamedFile(fileName.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
//    assert(result == KTX_SUCCESS);
//
//     = format;
//    newTexture->m_imageLayout = imageLayout;
//    newTexture->m_width = ktxTexture->baseWidth;
//    newTexture->m_height = ktxTexture->baseHeight;
//    newTexture->m_mipLevels = ktxTexture->numLevels;
//    newTexture->m_layerCount = ktxTexture->numLayers;
    
    ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
    
    if(copyRegion == TextureCopyRegion::Nothing)
    {
        texture->m_mipLevels = 1;
        texture->m_layerCount = 1;
    }
    else if(copyRegion == TextureCopyRegion::MipLevel)
    {
        texture->m_layerCount = 1;
    }
    else if(copyRegion == TextureCopyRegion::Layer)
    {
        texture->m_mipLevels = 1;
    }
    else if(copyRegion == TextureCopyRegion::Cube)
    {
//        newTexture->m_mipLevels = 1;
        texture->m_layerCount = 6;
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        
    }
    
    // Get device properties for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(Tools::m_physicalDevice, texture->m_fromat, &formatProperties);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    Tools::createBufferAndMemoryThenBind(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stagingBuffer, stagingMemory);
    Tools::mapMemory(stagingMemory, ktxTextureSize, ktxTextureData);
    
    VkImageCreateFlags flags = 0;
    uint32_t layerCount = texture->m_layerCount;
    
    if(copyRegion == TextureCopyRegion::Cube)
    {
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        layerCount = 6 * texture->m_layerCount;
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    
    Tools::createImageAndMemoryThenBind(texture->m_fromat, texture->m_width, texture->m_height, texture->m_mipLevels, layerCount,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        texture->m_image, texture->m_imageMemory, flags);
    
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    if(copyRegion == TextureCopyRegion::Nothing)
    {
        ktx_size_t offset;
        KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, 0, 0, 0, &offset);
        assert(ret == KTX_SUCCESS);
        // Setup a buffer image copy structure for the current array layer
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth;
        bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight;
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = offset;
        bufferCopyRegions.push_back(bufferCopyRegion);
    }
    else if(copyRegion == TextureCopyRegion::MipLevel)
    {
        for (uint32_t level = 0; level < texture->m_mipLevels; level++)
        {
            ktx_size_t offset;
            KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, 0, &offset);
            assert(result == KTX_SUCCESS);
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> level);
            bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> level);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }
    }
    else if(copyRegion == TextureCopyRegion::Layer)
    {
        for (uint32_t layer = 0; layer < texture->m_layerCount; layer++)
        {
            // Calculate offset into staging buffer for the current array layer
            ktx_size_t offset;
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, 0, layer, 0, &offset);
            assert(ret == KTX_SUCCESS);
            // Setup a buffer image copy structure for the current array layer
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = 0;
            bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth;
            bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }
    }
    else if(copyRegion == TextureCopyRegion::Cube)
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t level = 0; level < texture->m_mipLevels; level++)
            {
                // Calculate offset into staging buffer for the current mip level and face
                ktx_size_t offset;
                KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
                assert(ret == KTX_SUCCESS);
                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
                bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;
                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t layer = 0; layer < ktxTexture->numLayers; layer++)
            {
                for (uint32_t level = 0; level < ktxTexture->numLevels; level++)
                {
                    ktx_size_t offset;
                    KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, layer, face, &offset);
                    assert(ret == KTX_SUCCESS);
                    VkBufferImageCopy bufferCopyRegion = {};
                    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    bufferCopyRegion.imageSubresource.mipLevel = level;
                    bufferCopyRegion.imageSubresource.baseArrayLayer = layer * 6 + face;
                    bufferCopyRegion.imageSubresource.layerCount = 1;
                    bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
                    bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
                    bufferCopyRegion.imageExtent.depth = 1;
                    bufferCopyRegion.bufferOffset = offset;
                    bufferCopyRegions.push_back(bufferCopyRegion);
                }
            }
        }
    }
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = texture->m_mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = layerCount;
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Tools::setImageLayout(cmd, texture->m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

    vkCmdCopyBufferToImage(cmd, stagingBuffer, texture->m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

    Tools::setImageLayout(cmd, texture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          texture->m_imageLayout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

    Tools::flushCommandBuffer(cmd, transferQueue, true);
//    vkDeviceWaitIdle(Tools::m_device);
    
    ktxTexture_Destroy(ktxTexture);
    vkFreeMemory(Tools::m_device, stagingMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, stagingBuffer, nullptr);

    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    if(copyRegion == TextureCopyRegion::Layer)
    {
        viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
    else if(copyRegion == TextureCopyRegion::Cube)
    {
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
    
    Tools::createImageView(texture->m_image, texture->m_fromat, VK_IMAGE_ASPECT_COLOR_BIT, texture->m_mipLevels, layerCount, texture->m_imageView, viewType);
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, texture->m_mipLevels, texture->m_sampler);
}

Texture* Texture::loadTextrue2D(std::string fileName, VkQueue transferQueue, VkFormat format, TextureCopyRegion copyRegion, VkImageLayout imageLayout)
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
    newTexture->m_layerCount = ktxTexture->numLayers;
    ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
    
    if(copyRegion == TextureCopyRegion::Nothing)
    {
        newTexture->m_mipLevels = 1;
        newTexture->m_layerCount = 1;
    }
    else if(copyRegion == TextureCopyRegion::MipLevel)
    {
        newTexture->m_layerCount = 1;
    }
    else if(copyRegion == TextureCopyRegion::Layer)
    {
        newTexture->m_mipLevels = 1;
    }
    else if(copyRegion == TextureCopyRegion::Cube)
    {
//        newTexture->m_mipLevels = 1;
        newTexture->m_layerCount = 6;
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        
    }
    
    // Get device properties for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(Tools::m_physicalDevice, format, &formatProperties);
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    
    Tools::createBufferAndMemoryThenBind(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stagingBuffer, stagingMemory);
    Tools::mapMemory(stagingMemory, ktxTextureSize, ktxTextureData);
    
    VkImageCreateFlags flags = 0;
    uint32_t layerCount = newTexture->m_layerCount;
    
    if(copyRegion == TextureCopyRegion::Cube)
    {
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        layerCount = 6 * newTexture->m_layerCount;
        flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    
    Tools::createImageAndMemoryThenBind(format, newTexture->m_width, newTexture->m_height, newTexture->m_mipLevels, layerCount,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        newTexture->m_image, newTexture->m_imageMemory, flags);
    
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    if(copyRegion == TextureCopyRegion::Nothing)
    {
        ktx_size_t offset;
        KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, 0, 0, 0, &offset);
        assert(ret == KTX_SUCCESS);
        // Setup a buffer image copy structure for the current array layer
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth;
        bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight;
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = offset;
        bufferCopyRegions.push_back(bufferCopyRegion);
    }
    else if(copyRegion == TextureCopyRegion::MipLevel)
    {
        for (uint32_t level = 0; level < newTexture->m_mipLevels; level++)
        {
            ktx_size_t offset;
            KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, 0, &offset);
            assert(result == KTX_SUCCESS);
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = level;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> level);
            bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> level);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }
    }
    else if(copyRegion == TextureCopyRegion::Layer)
    {
        for (uint32_t layer = 0; layer < newTexture->m_layerCount; layer++)
        {
            // Calculate offset into staging buffer for the current array layer
            ktx_size_t offset;
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, 0, layer, 0, &offset);
            assert(ret == KTX_SUCCESS);
            // Setup a buffer image copy structure for the current array layer
            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = 0;
            bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth;
            bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }
    }
    else if(copyRegion == TextureCopyRegion::Cube)
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t level = 0; level < newTexture->m_mipLevels; level++)
            {
                // Calculate offset into staging buffer for the current mip level and face
                ktx_size_t offset;
                KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
                assert(ret == KTX_SUCCESS);
                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
                bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;
                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t layer = 0; layer < ktxTexture->numLayers; layer++)
            {
                for (uint32_t level = 0; level < ktxTexture->numLevels; level++)
                {
                    ktx_size_t offset;
                    KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, layer, face, &offset);
                    assert(ret == KTX_SUCCESS);
                    VkBufferImageCopy bufferCopyRegion = {};
                    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    bufferCopyRegion.imageSubresource.mipLevel = level;
                    bufferCopyRegion.imageSubresource.baseArrayLayer = layer * 6 + face;
                    bufferCopyRegion.imageSubresource.layerCount = 1;
                    bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
                    bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
                    bufferCopyRegion.imageExtent.depth = 1;
                    bufferCopyRegion.bufferOffset = offset;
                    bufferCopyRegions.push_back(bufferCopyRegion);
                }
            }
        }
    }
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = newTexture->m_mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = layerCount;
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Tools::setImageLayout(cmd, newTexture->m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

    vkCmdCopyBufferToImage(cmd, stagingBuffer, newTexture->m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

    Tools::setImageLayout(cmd, newTexture->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          newTexture->m_imageLayout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);

    Tools::flushCommandBuffer(cmd, transferQueue, true);
//    vkDeviceWaitIdle(Tools::m_device);
    
    ktxTexture_Destroy(ktxTexture);
    vkFreeMemory(Tools::m_device, stagingMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, stagingBuffer, nullptr);

    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    if(copyRegion == TextureCopyRegion::Layer)
    {
        viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
    else if(copyRegion == TextureCopyRegion::Cube)
    {
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    }
    else if(copyRegion == TextureCopyRegion::CubeArry)
    {
        viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
    
    Tools::createImageView(newTexture->m_image, format, VK_IMAGE_ASPECT_COLOR_BIT, newTexture->m_mipLevels, layerCount, newTexture->m_imageView, viewType);
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, newTexture->m_mipLevels, newTexture->m_sampler);
    return newTexture;
}


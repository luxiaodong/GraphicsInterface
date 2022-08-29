
#include "tools.h"
#include "common/svpng.inc"
#include <stdlib.h>
#include <random>

VkPhysicalDevice Tools::m_physicalDevice = VK_NULL_HANDLE;
VkDevice Tools::m_device = VK_NULL_HANDLE;
VkQueue Tools::m_graphicsQueue = VK_NULL_HANDLE;
VkQueue Tools::m_computerQueue = VK_NULL_HANDLE;
VkCommandPool Tools::m_commandPool = VK_NULL_HANDLE;
VkPhysicalDeviceFeatures Tools::m_deviceEnabledFeatures = {};
VkPhysicalDeviceProperties Tools::m_deviceProperties = {};
bool Tools::m_isLowEndian = false;

std::default_random_engine rndEngine;

void Tools::init()
{
    // 检查大小端
    union w
    {
        int a;
        char b;
    }c;
    c.a = 1;
    Tools::m_isLowEndian = (c.b == 1);  // 小端返回TRUE,大端返回FALSE
}

void Tools::seed()
{
    rndEngine.seed((unsigned)time(nullptr));
}

float Tools::random01()
{
    std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
    return rndDist(rndEngine);
}

float Tools::lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

std::string Tools::getShaderPath()
{
    return "assets/shaders/";
}

std::string Tools::getModelPath()
{
    return "assets/models/";
}

std::string Tools::getTexturePath()
{
    return "assets/textures/";
}

bool Tools::isFileExists(const std::string &filename)
{
    std::ifstream f(filename.c_str());
    return !f.fail();
}

std::vector<char> Tools::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
       throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule Tools::createShaderModule(const std::string& filename)
{
    std::vector<char> code = readFile(filename);
    
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    if( vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

VkFormat Tools::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

void Tools::createBufferAndMemoryThenBind(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, VkBuffer &buffer, VkDeviceMemory& memory)
{
    VkBufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    if(vkCreateBuffer(m_device, &createInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size; // > createInfo.size, need alignment
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, flags);

    if( vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate memory!");
    }

    //buffer相当于memory的头信息, memory是真正的内存.
    vkBindBufferMemory(m_device, buffer, memory, 0);
}

void Tools::createImageAndMemoryThenBind(VkFormat format, uint32_t width, uint32_t height, uint32_t lodLevels, uint32_t layerCount, VkSampleCountFlagBits sampleFlag, VkImageUsageFlags usage, VkImageTiling tiling, VkMemoryPropertyFlags propertyFlags, VkImage &image, VkDeviceMemory &imageMemory, VkImageCreateFlags createFlags, uint32_t depth, VkImageType imageType)
{
    VkImageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.flags = createFlags;
    createInfo.imageType = imageType;
    createInfo.format = format;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = depth;
    createInfo.mipLevels = lodLevels;
    createInfo.arrayLayers = layerCount;
    createInfo.samples = sampleFlag;
    createInfo.tiling = tiling;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    std::vector<uint32_t> queueFamilyIndices;
    queueFamilyIndices.push_back(0);
    createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
    createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if( vkCreateImage(m_device, &createInfo, nullptr, &image) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create iamge!");
    }
            
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, propertyFlags);

    if( vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate memory!");
    }
    
    if( vkBindImageMemory(m_device, image, imageMemory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to bind image memory!");
    }
}

uint32_t Tools::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
    
    for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if( (typeFilter & (1<<i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) )
        {
            return i;
        }
    }
    
    throw std::runtime_error("failed to find suitable memory type!");
}

void Tools::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t levelCount, uint32_t layerCount, VkImageView &imageView, VkImageViewType viewType, uint32_t baseArrayLayer)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.image = image;
    createInfo.viewType = viewType;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspectFlags;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = levelCount;
    createInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
    createInfo.subresourceRange.layerCount = layerCount;

    if( vkCreateImageView(m_device, &createInfo, nullptr, &imageView) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create imageView!");
    }
}

void Tools::mapMemory(VkDeviceMemory &memory, VkDeviceSize size, void* srcAddress, VkDeviceSize offset)
{
    void* dstAddress;
    vkMapMemory(m_device, memory, offset, size, 0, &dstAddress);
    memcpy(dstAddress, srcAddress, size);
    vkUnmapMemory(m_device, memory);
}

VkCommandBuffer Tools::createCommandBuffer(VkCommandBufferLevel level, bool isBegin, uint32_t count)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = level;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = count;

    VkCommandBuffer commandBuffer;
    if(vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    
    if (isBegin)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin command buffer!");
        }
    }

    return commandBuffer;
}

void Tools::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool isFree)
{
    if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to end command buffer!");
    }
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    
    VkFence fence;
    if(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create fence!");
    }
    
    if(vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to queue submit!");
    }
    
    if(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to wait for fences!");
    }
    vkDestroyFence(m_device, fence, nullptr);

    if(isFree)
    {
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    }
}

void Tools::createTextureSampler(VkFilter filter, VkSamplerAddressMode addressMode, uint32_t maxLod, VkSampler &sampler)
{
    VkSamplerCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.magFilter = filter;
    createInfo.minFilter = filter;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.addressModeU = addressMode;
    createInfo.addressModeV = addressMode;
    createInfo.addressModeW = addressMode;
    createInfo.mipLodBias = 0.0f;
    
    if(m_deviceEnabledFeatures.samplerAnisotropy == VK_TRUE)
    {
        createInfo.anisotropyEnable = VK_TRUE;
        createInfo.maxAnisotropy = m_deviceProperties.limits.maxSamplerAnisotropy;
    }
    else
    {
        createInfo.anisotropyEnable = VK_FALSE;
        createInfo.maxAnisotropy = 1.0f;
    }
    
    createInfo.compareEnable = VK_FALSE;
    createInfo.compareOp = VK_COMPARE_OP_NEVER;
    createInfo.minLod = 0;
    createInfo.maxLod = maxLod;
    createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; //寻址模式是 VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER 时设置
    createInfo.unnormalizedCoordinates = VK_FALSE; // false,(0,1), true,(0,width/height)
    
    if( vkCreateSampler(m_device, &createInfo, nullptr, &sampler) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create sampler!");
    }
}

VkDescriptorSetLayoutCreateInfo Tools::getDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding* pBindings, uint32_t bindingCount)
{
    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pBindings = pBindings;
    createInfo.bindingCount = bindingCount;
    return createInfo;
}

void Tools::allocateDescriptorSets(VkDescriptorPool descriptorPool, VkDescriptorSetLayout* pSetLayouts, uint32_t descriptorSetCount, VkDescriptorSet& descriptorSet)
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = pSetLayouts;
    
    if( vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptorSets!");
    }
}

VkDescriptorSetLayoutBinding Tools::getDescriptorSetLayoutBinding(VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t count)
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
    descriptorSetLayoutBinding.binding = binding;
    descriptorSetLayoutBinding.descriptorType = descriptorType;
    descriptorSetLayoutBinding.descriptorCount = count;
    descriptorSetLayoutBinding.stageFlags = stageFlags;
    descriptorSetLayoutBinding.pImmutableSamplers = nullptr;
    return descriptorSetLayoutBinding;
}

VkWriteDescriptorSet Tools::getWriteDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType descriptorType, uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t count)
{
    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorCount = count;
    writeDescriptorSet.descriptorType = descriptorType;
    writeDescriptorSet.pBufferInfo = bufferInfo;
    writeDescriptorSet.pImageInfo = nullptr;
    writeDescriptorSet.pTexelBufferView = nullptr;
    return writeDescriptorSet;
}

VkWriteDescriptorSet Tools::getWriteDescriptorSet(VkDescriptorSet descriptorSet, VkDescriptorType descriptorType, uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t count)
{
    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorCount = count;
    writeDescriptorSet.descriptorType = descriptorType;
    writeDescriptorSet.pBufferInfo = nullptr;
    writeDescriptorSet.pImageInfo = imageInfo;
    writeDescriptorSet.pTexelBufferView = nullptr;
    return writeDescriptorSet;
}

VkAttachmentDescription Tools::getAttachmentDescription(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
    VkAttachmentDescription attachmentDescription = {};
    attachmentDescription.flags = 0;
    attachmentDescription.format = format;
    attachmentDescription.samples = samples;
    attachmentDescription.loadOp = loadOp;
    attachmentDescription.storeOp = storeOp;
    attachmentDescription.stencilLoadOp = stencilLoadOp;
    attachmentDescription.stencilStoreOp = stencilStoreOp;
    attachmentDescription.initialLayout = initialLayout;
    attachmentDescription.finalLayout = finalLayout;
    return attachmentDescription;
}

VkPipelineShaderStageCreateInfo Tools::getPipelineShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage = stage;
    createInfo.module = module;
    createInfo.pName = "main";
    return createInfo;
}

VkVertexInputBindingDescription Tools::getVertexInputBindingDescription(uint32_t binding, uint32_t stride)
{
    VkVertexInputBindingDescription des = {};
    des.binding = binding;
    des.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    des.stride = stride;
    return des;
}

VkVertexInputAttributeDescription Tools::getVertexInputAttributeDescription(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset)
{
    VkVertexInputAttributeDescription des = {};
    des.binding = binding;
    des.location = location;
    des.format = format;
    des.offset = offset;
    return des;
}

VkPipelineVertexInputStateCreateInfo Tools::getPipelineVertexInputStateCreateInfo(std::vector<VkVertexInputBindingDescription> &bindingDescriptions, std::vector<VkVertexInputAttributeDescription> &attributeDescriptions)
{
    VkPipelineVertexInputStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    createInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    createInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    createInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    return createInfo;
}

VkPipelineInputAssemblyStateCreateInfo Tools::getPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
{
    VkPipelineInputAssemblyStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.primitiveRestartEnable = primitiveRestartEnable;
    createInfo.topology = topology;
    return createInfo;
}

VkPipelineTessellationStateCreateInfo Tools::getPipelineTessellationStateCreateInfo(uint32_t patchControlPoints)
{
    VkPipelineTessellationStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.patchControlPoints = patchControlPoints;
    return createInfo;
}

VkViewport Tools::getViewport(float x, float y, float width, float height)
{
    VkViewport viewport;
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    return viewport;
}

VkPipelineViewportStateCreateInfo Tools::getPipelineViewportStateCreateInfo( std::vector<VkViewport>& viewPorts, std::vector<VkRect2D>& scissors)
{
    VkPipelineViewportStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.viewportCount = static_cast<uint32_t>(viewPorts.size());
    createInfo.pViewports = viewPorts.data();
    createInfo.scissorCount = static_cast<uint32_t>(scissors.size());
    createInfo.pScissors = scissors.data();
    return createInfo;
}

VkPipelineRasterizationStateCreateInfo Tools::getPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlagBits cullMode, VkFrontFace frontFace)
{
    VkPipelineRasterizationStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.depthClampEnable = VK_FALSE;
    createInfo.rasterizerDiscardEnable = VK_FALSE;
    createInfo.polygonMode = polygonMode;
    createInfo.cullMode = cullMode;
    createInfo.frontFace = frontFace;
    createInfo.depthBiasEnable = VK_FALSE;
    createInfo.depthBiasConstantFactor = 0.0f;
    createInfo.depthBiasClamp = VK_FALSE;
    createInfo.depthBiasSlopeFactor = 0.0f;
    createInfo.lineWidth = 1.0f;
    return createInfo;
}

VkPipelineMultisampleStateCreateInfo Tools::getPipelineMultisampleStateCreateInfo(VkSampleCountFlagBits sampleCount)
{
    VkPipelineMultisampleStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.rasterizationSamples = sampleCount;
    createInfo.sampleShadingEnable = VK_FALSE;
    createInfo.pSampleMask = nullptr;
    createInfo.alphaToCoverageEnable = VK_FALSE;
    createInfo.alphaToOneEnable = VK_FALSE;
    return createInfo;
}

VkPipelineDepthStencilStateCreateInfo Tools::getPipelineDepthStencilStateCreateInfo(VkBool32 depthTest, VkBool32 depthWrite, VkCompareOp compareOp)
{
    VkPipelineDepthStencilStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.depthTestEnable = depthTest;
    createInfo.depthWriteEnable = depthWrite;
    createInfo.depthCompareOp = compareOp;
    createInfo.depthBoundsTestEnable = VK_FALSE;
    createInfo.stencilTestEnable = VK_FALSE;
    createInfo.minDepthBounds = 0.0f;
    createInfo.minDepthBounds = 1.0f;
    return createInfo;
}

VkPipelineColorBlendAttachmentState Tools::getPipelineColorBlendAttachmentState(VkBool32 blend, VkColorComponentFlags colorComponent)
{
    VkPipelineColorBlendAttachmentState state = {};
    state.blendEnable = blend;
    state.colorWriteMask = colorComponent;
    state.colorBlendOp = VK_BLEND_OP_ADD;
    state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.alphaBlendOp = VK_BLEND_OP_ADD;
    state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    return state;
}

VkPipelineColorBlendStateCreateInfo Tools::getPipelineColorBlendStateCreateInfo(std::vector<VkPipelineColorBlendAttachmentState>& colorBlendAttachments)
{
    uint32_t count = static_cast<uint32_t>(colorBlendAttachments.size());
    VkPipelineColorBlendAttachmentState *pColorBlendAttachment = colorBlendAttachments.data();
    return getPipelineColorBlendStateCreateInfo(count, pColorBlendAttachment);
}

VkPipelineColorBlendStateCreateInfo Tools::getPipelineColorBlendStateCreateInfo(uint32_t count, VkPipelineColorBlendAttachmentState* pColorBlendAttachment)
{
    VkPipelineColorBlendStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.logicOpEnable = VK_FALSE;
    createInfo.logicOp = VK_LOGIC_OP_COPY;
    createInfo.attachmentCount = count;
    createInfo.pAttachments = pColorBlendAttachment;
    return createInfo;
}

VkPipelineDynamicStateCreateInfo Tools::getPipelineDynamicStateCreateInfo(std::vector<VkDynamicState>& dynamicStates)
{
    uint32_t count = static_cast<uint32_t>(dynamicStates.size());
    VkDynamicState *pDynamicState = dynamicStates.data();
    return getPipelineDynamicStateCreateInfo(count, pDynamicState);
}

VkPipelineDynamicStateCreateInfo Tools::getPipelineDynamicStateCreateInfo(uint32_t count, VkDynamicState* pDynamicState)
{
    VkPipelineDynamicStateCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.dynamicStateCount = count;
    createInfo.pDynamicStates = pDynamicState;
    return createInfo;
}

VkGraphicsPipelineCreateInfo Tools::getGraphicsPipelineCreateInfo(VkPipelineLayout layout, VkRenderPass renderPass)
{
    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.layout = layout;
    createInfo.renderPass = renderPass;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;
    return createInfo;
}

VkGraphicsPipelineCreateInfo Tools::getGraphicsPipelineCreateInfo(std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, VkPipelineLayout layout, VkRenderPass renderPass)
{
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(layout, renderPass);
    
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    return createInfo;
}

void Tools::setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = aspectMask;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    setImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, srcStageMask, dstStageMask, subresourceRange);
}

void Tools::setImageLayout( VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange)
{
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (oldImageLayout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        // Image layout is undefined (or does not matter)
        // Only valid as initial layout
        // No flags required, listed only for completeness
        imageMemoryBarrier.srcAccessMask = 0;
        break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        // Image is preinitialized
        // Only valid as initial layout for linear images, preserves memory contents
        // Make sure host writes have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image is a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image is a depth/stencil attachment
        // Make sure any writes to the depth/stencil buffer have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image is a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image is a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image is read by a shader
        // Make sure any shader reads from the image have been finished
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Target layouts (new)
    // Destination access mask controls the dependency for the new image layout
    switch (newImageLayout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        // Image will be used as a transfer destination
        // Make sure any writes to the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        // Image will be used as a transfer source
        // Make sure any reads from the image have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        // Image will be used as a color attachment
        // Make sure any writes to the color buffer have been finished
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        // Image layout will be used as a depth/stencil attachment
        // Make sure any writes to depth/stencil buffer have been finished
        imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // Image will be read in a shader (sampler, input attachment)
        // Make sure any writes to the image have been finished
        if (imageMemoryBarrier.srcAccessMask == 0)
        {
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    default:
        // Other source layouts aren't handled (yet)
        break;
    }

    // Put barrier inside setup command buffer
    vkCmdPipelineBarrier(
        cmdbuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

VkSampleCountFlagBits Tools::getMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void Tools::saveImage(const VkImage& srcImage, VkFormat srcFormat, VkImageLayout oldlayout, uint32_t width, uint32_t height, const std::string filePath)
{
//    VkImage srcImage = m_swapchainImages[m_imageIndex];
    VkImage dstImage;
    VkDeviceMemory dstMemory;
    VkFormat dstFormat = srcFormat;
    
    Tools::createImageAndMemoryThenBind(dstFormat, width, height, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_TILING_LINEAR,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        dstImage, dstMemory);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    Tools::setImageLayout(cmd, dstImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
    subresourceRange.levelCount = 1;
    subresourceRange.baseMipLevel = 0;
    
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = subresourceRange;
    barrier.image = srcImage;
    barrier.oldLayout = oldlayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    barrier.image = dstImage;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    bool useBlitImage = true;
    
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, srcFormat, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) useBlitImage = false;

    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_B8G8R8A8_UNORM, &formatProps);
    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) useBlitImage = false;
    
    if(useBlitImage)
    {
        VkImageBlit imageBlit = {};
        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.srcSubresource.mipLevel = 0;
        imageBlit.srcSubresource.baseArrayLayer = 0;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.dstSubresource.mipLevel = 0;
        imageBlit.dstSubresource.baseArrayLayer = 0;
        imageBlit.dstSubresource.layerCount = 1;
        imageBlit.srcOffsets[0] = {0,0,0};
        imageBlit.srcOffsets[1] = {(int)width, (int)height, 1};
        imageBlit.dstOffsets[0] = {0,0,0};
        imageBlit.dstOffsets[1] = {(int)width, (int)height, 1};
        
        vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
    }
    else
    {
        VkImageCopy imageCopyRegion{};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = width;
        imageCopyRegion.extent.height = height;
        imageCopyRegion.extent.depth = 1;
        vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);
    }
    
    barrier.image = srcImage;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = oldlayout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    barrier.image = dstImage;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    
    // 获取数据的偏移地址.
    VkImageSubresource subResource = {};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.mipLevel = 0;
    subResource.arrayLayer = 0;
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(m_device, dstImage, &subResource, &subResourceLayout);
    
    unsigned char *pDstImage;
    vkMapMemory(m_device, dstMemory, 0, VK_WHOLE_SIZE, 0, (void**)&pDstImage);
    pDstImage += subResourceLayout.offset;
    
    if(dstFormat == VK_FORMAT_R8G8B8A8_UNORM)
    {
        svpng(fopen(filePath.c_str(), "wb"), width, height, pDstImage, 1);
    }
    else
    {
        unsigned char* img = new unsigned char[width * height * 4];
        unsigned char* pOutImage = img;
        
        for(uint32_t j=0; j<height; ++j)
        {
            for(uint32_t i=0; i<width; ++i)
            {
                if(VK_FORMAT_B8G8R8A8_UNORM == dstFormat)
                {
                    uint32_t offset = i*4;
                    *pOutImage++ = *(pDstImage+offset+2);
                    *pOutImage++ = *(pDstImage+offset+1);
                    *pOutImage++ = *(pDstImage+offset);
                    *pOutImage++ = *(pDstImage+offset+3);
                }
                else if(VK_FORMAT_R16G16_SFLOAT == dstFormat)
                {
                    uint32_t offset = i*4;
                    float r = Tools::float16(*(pDstImage+offset+1), *(pDstImage+offset));
                    float g = Tools::float16(*(pDstImage+offset+3), *(pDstImage+offset+2));
                    if(r > 1) r = 1;
                    if(g > 1) g = 1;
                    *pOutImage++ = r*255;
                    *pOutImage++ = g*255;
                    *pOutImage++ = 0;
                    *pOutImage++ = 255;
                }
                else if(VK_FORMAT_R16G16B16A16_SFLOAT == dstFormat)
                {
                    uint32_t offset = i*8;
                    float r = Tools::float16(*(pDstImage+offset+1), *(pDstImage+offset));
                    float g = Tools::float16(*(pDstImage+offset+3), *(pDstImage+offset+2));
                    float b = Tools::float16(*(pDstImage+offset+5), *(pDstImage+offset+4));
                    float a = Tools::float16(*(pDstImage+offset+7), *(pDstImage+offset+6));
                    if(r > 1) r = 1;
                    if(g > 1) g = 1;
                    if(b > 1) b = 1;
                    if(a > 1) a = 1;
                    *pOutImage++ = r*255;
                    *pOutImage++ = g*255;
                    *pOutImage++ = b*255;
                    *pOutImage++ = a*255;
                }
            }
            pDstImage += subResourceLayout.rowPitch;
        }
        
        svpng(fopen(filePath.c_str(), "wb"), width, height, img, 1);
        delete[] img;
    }
    
    vkUnmapMemory(m_device, dstMemory);
    vkFreeMemory(m_device, dstMemory, nullptr);
    vkDestroyImage(m_device, dstImage, nullptr);
}

float Tools::float16(unsigned char high, unsigned char low)
{
    if(Tools::m_isLowEndian == false)
    {
        std::swap(high, low);
    }
    
    //1 + 5 + 10
    int sign = 0x80 & high;
    int exp = ((0x7c & high) >> 2) - 15;
    int precision = (high & 0x3);
    precision = (precision << 8) + low;
    float p = (1 + precision*1.0f/1024) * pow(2.0f, exp);
    if (sign) p = -p;
    return p;
}

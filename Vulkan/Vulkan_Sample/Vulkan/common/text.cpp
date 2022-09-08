
#include "text.h"

#define TEXTOVERLAY_MAX_CHAR_COUNT 2048

Text::Text(VkFormat colorformat, VkFormat depthformat, uint32_t framebufferWidth, uint32_t framebufferHeight, std::string vertFilePath, std::string fragFilePath)
{
    this->m_colorFormat = colorformat;
    this->m_depthFormat = depthformat;
    this->m_framebufferWidth = framebufferWidth;
    this->m_framebufferHeight = framebufferHeight;
    this->m_vertFilePath = vertFilePath;
    this->m_fragFilePath = fragFilePath;
}

Text::~Text()
{
}

void Text::init()
{
    prepareResources();
    createRenderPass();
    createPipeline();
}

void Text::clear()
{
    vkDestroyPipeline(Tools::m_device, m_graphicsPipeline, nullptr);
    vkDestroyRenderPass(Tools::m_device, m_renderPass, nullptr);
    vkDestroyPipelineLayout(Tools::m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(Tools::m_device, m_descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(Tools::m_device, m_descriptorPool, nullptr);
    
    vkDestroySampler(Tools::m_device, m_fontSampler, nullptr);
    vkDestroyImageView(Tools::m_device, m_fontImageView, nullptr);
    vkDestroyImage(Tools::m_device, m_fontImage, nullptr);
    vkFreeMemory(Tools::m_device, m_fontMemory, nullptr);
    
    vkDestroyBuffer(Tools::m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(Tools::m_device, m_vertexMemory, nullptr);
    
    vkDestroyCommandPool(Tools::m_device, m_commandPool, nullptr);
}

void Text::prepareResources()
{
    const uint32_t fontWidth = STB_FONT_consolas_24_latin1_BITMAP_WIDTH;
    const uint32_t fontHeight = STB_FONT_consolas_24_latin1_BITMAP_WIDTH;
    static unsigned char font24pixels[fontWidth][fontHeight];
    stb_font_consolas_24_latin1(m_stbFontData, font24pixels, fontHeight);
    
    unsigned char* fontData = &font24pixels[0][0];

    {
        VkDeviceSize vertexSize = TEXTOVERLAY_MAX_CHAR_COUNT * sizeof(glm::vec4);
        Tools::createBufferAndMemoryThenBind(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                             m_vertexBuffer, m_vertexMemory);
        
        // font
        VkDeviceSize fontSize = fontWidth * fontHeight;
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        Tools::createBufferAndMemoryThenBind(fontSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                             stagingBuffer, stagingMemory);
        
        Tools::mapMemory(stagingMemory, fontSize, fontData);
        
        Tools::createImageAndMemoryThenBind(VK_FORMAT_R8_UNORM, fontWidth, fontHeight, 1, 1,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_fontImage, m_fontMemory);
        
        // copy
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tools::setImageLayout(cmd, m_fontImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = fontWidth;
        region.imageExtent.height = fontHeight;
        region.imageExtent.depth = 1;
        
        vkCmdCopyBufferToImage(cmd, stagingBuffer, m_fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        Tools::setImageLayout(cmd, m_fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        
        Tools::flushCommandBuffer(cmd, Tools::m_graphicsQueue, true);
        
        Tools::createImageView(m_fontImage, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_fontImageView);
        Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_fontSampler);
        
        vkFreeMemory(Tools::m_device, stagingMemory, nullptr);
        vkDestroyBuffer(Tools::m_device, stagingBuffer, nullptr);
    }
    
    // descriptor
    {
        VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        
        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.bindingCount = 1;
        createInfo.pBindings = &binding;
        
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(Tools::m_device, &createInfo, nullptr, &m_descriptorSetLayout));
    }
    
    {
        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.setLayoutCount = 1;
        createInfo.pSetLayouts = &m_descriptorSetLayout;
        
        VK_CHECK_RESULT(vkCreatePipelineLayout(Tools::m_device, &createInfo, nullptr, &m_pipelineLayout));
    }
    
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;
        
        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.poolSizeCount = 1;
        createInfo.pPoolSizes = &poolSize;
        createInfo.maxSets = 1;
        VK_CHECK_RESULT(vkCreateDescriptorPool(Tools::m_device, &createInfo, nullptr, &m_descriptorPool));
    }
    
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;
        
        VK_CHECK_RESULT(vkAllocateDescriptorSets(Tools::m_device, &allocInfo, &m_descriptorSet));
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageView = m_fontImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.sampler = m_fontSampler;
        
        VkWriteDescriptorSet writeDescriptorSet = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo);
        vkUpdateDescriptorSets(Tools::m_device, 1, &writeDescriptorSet, 0, nullptr);
    }
    
    {
        VkCommandPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.queueFamilyIndex = 0;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK_RESULT(vkCreateCommandPool(Tools::m_device, &createInfo, nullptr, &m_commandPool));
        
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK_RESULT(vkAllocateCommandBuffers(Tools::m_device, &allocInfo, &m_commandBuffer));
    }
}

void Text::createRenderPass()
{
    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = m_colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[1].format = m_depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDependency subpassDependencies[2] = {};
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.flags = 0;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = NULL;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pResolveAttachments = NULL;
    subpassDescription.pDepthStencilAttachment = &depthReference;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = NULL;
    
    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.attachmentCount = 2;
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = 2;
    createInfo.pDependencies = subpassDependencies;
    
    VK_CHECK_RESULT(vkCreateRenderPass(Tools::m_device, &createInfo, nullptr, &m_renderPass));
}

void Text::createPipeline()
{
    std::array<VkVertexInputBindingDescription, 2> vertexInputBindings = {};
    vertexInputBindings[0] = Tools::getVertexInputBindingDescription(0, sizeof(glm::vec4));
    vertexInputBindings[1] = Tools::getVertexInputBindingDescription(1, sizeof(glm::vec4));
    
    std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributes = {};
    vertexInputAttributes[0] = Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
    vertexInputAttributes[1] = Tools::getVertexInputAttributeDescription(1, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec2));
    
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInput.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInput.pVertexAttributeDescriptions = vertexInputAttributes.data();
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = Tools::getPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE);
    
    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.flags = 0;
    viewport.viewportCount = 1;
    viewport.pViewports = nullptr;
    viewport.scissorCount = 1;
    viewport.pScissors = nullptr;
    
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamic = Tools::getPipelineDynamicStateCreateInfo(dynamicStates);
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_TRUE, 0xf);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    createInfo.pVertexInputState = &vertexInput;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    VkShaderModule vertModule = Tools::createShaderModule(m_vertFilePath);
    VkShaderModule fragModule = Tools::createShaderModule(m_fragFilePath);
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(Tools::m_device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_graphicsPipeline));
    vkDestroyShaderModule(Tools::m_device, vertModule, nullptr);
    vkDestroyShaderModule(Tools::m_device, fragModule, nullptr);
}

void Text::begin()
{
    VK_CHECK_RESULT(vkMapMemory(Tools::m_device, m_vertexMemory, 0, VK_WHOLE_SIZE, 0, (void **)&mapped));
    m_numLetter = 0;
}

void Text::addString(std::string str, float x, float y, TextAlign align)
{
    const uint32_t firstChar = STB_FONT_consolas_24_latin1_FIRST_CHAR;
    
    const float charW = 1.5f / m_framebufferWidth;
    const float charH = 1.5f / m_framebufferHeight;
    
    x = (x / m_framebufferWidth * 2.0f) - 1.0f;
    y = (y / m_framebufferHeight * 2.0f) - 1.0f;
    
    // Calculate text width
    float textWidth = 0;
    for (auto letter : str)
    {
        stb_fontchar *charData = &m_stbFontData[(uint32_t)letter - firstChar];
        textWidth += charData->advance * charW;
    }
    
    if(align == TextAlign::Center)
    {
        x -= textWidth;
    }
    else if(align == TextAlign::Right)
    {
        x -= textWidth/2.0f;
    }
    
    // Generate a uv mapped quad per char in the new text
    for (auto letter : str)
    {
        stb_fontchar *charData = &m_stbFontData[(uint32_t)letter - firstChar];

        mapped->x = (x + (float)charData->x0 * charW);
        mapped->y = (y + (float)charData->y0 * charH);
        mapped->z = charData->s0;
        mapped->w = charData->t0;
        mapped++;

        mapped->x = (x + (float)charData->x1 * charW);
        mapped->y = (y + (float)charData->y0 * charH);
        mapped->z = charData->s1;
        mapped->w = charData->t0;
        mapped++;

        mapped->x = (x + (float)charData->x0 * charW);
        mapped->y = (y + (float)charData->y1 * charH);
        mapped->z = charData->s0;
        mapped->w = charData->t1;
        mapped++;

        mapped->x = (x + (float)charData->x1 * charW);
        mapped->y = (y + (float)charData->y1 * charH);
        mapped->z = charData->s1;
        mapped->w = charData->t1;
        mapped++;

        x += charData->advance * charW;

        m_numLetter++;
    }
}

void Text::end()
{
    vkUnmapMemory(Tools::m_device, m_vertexMemory);
    mapped = nullptr;
}

void Text::updateCommandBuffers(const VkFramebuffer& frameBuffer)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_RESULT(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));
    
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_renderPass;
    passBeginInfo.framebuffer = frameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = m_framebufferWidth;
    passBeginInfo.renderArea.extent.height = m_framebufferHeight;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(m_commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    this->recordRenderCommand(m_commandBuffer);
    
    vkCmdEndRenderPass(m_commandBuffer);
    VK_CHECK_RESULT(vkEndCommandBuffer(m_commandBuffer));
}

void Text::recordRenderCommand(const VkCommandBuffer& commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = m_framebufferWidth;
    scissor.extent.height = m_framebufferHeight;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    
    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offsets);
    vkCmdBindVertexBuffers(commandBuffer, 1, 1, &m_vertexBuffer, &offsets);
    
    for (uint32_t j = 0; j < m_numLetter; j++)
    {
        vkCmdDraw(commandBuffer, 4, 1, j * 4, 0);
    }
}
                        

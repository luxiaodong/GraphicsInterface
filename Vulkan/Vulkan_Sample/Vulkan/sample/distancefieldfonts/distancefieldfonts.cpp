
#include "distancefieldfonts.h"

DistanceFieldFonts::DistanceFieldFonts(std::string title) : Application(title)
{
}

DistanceFieldFonts::~DistanceFieldFonts()
{}

void DistanceFieldFonts::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void DistanceFieldFonts::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(30.0f, (float)m_width / (float)(m_height * 0.5f), 1.0f, 256.0f);
}

void DistanceFieldFonts::setEnabledFeatures()
{
//    if( m_deviceFeatures.fillModeNonSolid )
//    {
//        m_deviceEnabledFeatures.fillModeNonSolid = VK_TRUE;
//        if( m_deviceFeatures.wideLines )
//        {
//            m_deviceEnabledFeatures.wideLines = VK_TRUE;
//        }
//    }
}

void DistanceFieldFonts::clear()
{
    vkDestroyPipeline(m_device, m_fontBmpPipeline, nullptr);
    vkDestroyPipeline(m_device, m_fontSdfPipeline, nullptr);
    
    vkFreeMemory(m_device, m_uniformVertMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformVertBuffer, nullptr);
    vkFreeMemory(m_device, m_uniformFragMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformFragBuffer, nullptr);
    vkFreeMemory(m_device, m_vertexMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexMemory, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);

    m_pFontBmp->clear();
    m_pFontSdf->clear();
    delete m_pFontBmp;
    delete m_pFontSdf;
    Application::clear();
}

void DistanceFieldFonts::prepareVertex()
{
    m_pFontSdf = Texture::loadTextrue2D(Tools::getTexturePath() +  "font_sdf_rgba.ktx", m_graphicsQueue);
    m_pFontBmp = Texture::loadTextrue2D(Tools::getTexturePath() +  "font_bitmap_rgba.ktx", m_graphicsQueue);
    
    m_vertexInputBindDes.clear();
    m_vertexInputBindDes.push_back(Tools::getVertexInputBindingDescription(0, sizeof(FontVertex)));
    m_vertexInputAttrDes.clear();
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FontVertex, pos)));
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(FontVertex, uv)));
    
    parseBmpFont();
    generateText("Vulkan");
}

void DistanceFieldFonts::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(UniformVert);

    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformVertBuffer, m_uniformVertMemory);
    
    UniformVert mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    Tools::mapMemory(m_uniformVertMemory, sizeof(UniformVert), &mvp);
    
    
    uniformSize = sizeof(UniformFrag);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformFragBuffer, m_uniformFragMemory);
    UniformFrag data = {};
    data.outlineColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    data.outlineWidth = 0.6f;
    data.outline = true;
    Tools::mapMemory(m_uniformFragMemory, sizeof(UniformFrag), &data);
}

void DistanceFieldFonts::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
    
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void DistanceFieldFonts::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    
    {
        createDescriptorSet(m_fontSdfDescriptorSet);

        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = sizeof(UniformVert);
        bufferInfo1.buffer = m_uniformVertBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pFontSdf->getDescriptorImageInfo();
        
        VkDescriptorBufferInfo bufferInfo2 = {};
        bufferInfo2.offset = 0;
        bufferInfo2.range = sizeof(UniformFrag);
        bufferInfo2.buffer = m_uniformFragBuffer;

        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_fontSdfDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_fontSdfDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        writes[2] = Tools::getWriteDescriptorSet(m_fontSdfDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &bufferInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_fontBmpDescriptorSet);

        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = sizeof(UniformVert);
        bufferInfo1.buffer = m_uniformVertBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pFontBmp->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_fontBmpDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_fontBmpDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void DistanceFieldFonts::createGraphicsPipeline()
{
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindDes.size());
    vertexInput.pVertexBindingDescriptions = m_vertexInputBindDes.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttrDes.size());
    vertexInput.pVertexAttributeDescriptions = m_vertexInputAttrDes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = Tools::getPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

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
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_TRUE, 0xf);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

//    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

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

    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "distancefieldfonts/sdf.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "distancefieldfonts/sdf.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_fontSdfPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "distancefieldfonts/bitmap.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "distancefieldfonts/bitmap.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_fontBmpPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void DistanceFieldFonts::updateRenderData()
{
    
}

void DistanceFieldFonts::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height * 0.5f);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fontSdfPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_fontSdfDescriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
    
    viewport.y += m_swapchainExtent.height * 0.5f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fontBmpPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_fontBmpDescriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
}

int32_t nextValuePair(std::stringstream *stream)
{
    std::string pair;
    *stream >> pair;
    uint32_t spos = static_cast<uint32_t>(pair.find("="));
    std::string value = pair.substr(spos + 1);
    int32_t val = std::stoi(value);
    return val;
}

// http://www.angelcode.com/products/bmfont/doc/file_format.html
void DistanceFieldFonts::parseBmpFont()
{
    const std::string fileName = "assets/font.fnt";
    
    std::filebuf fileBuffer;
    fileBuffer.open(fileName, std::ios::in);
    std::istream istream(&fileBuffer);

    assert(istream.good());

    while (!istream.eof())
    {
        std::string line;
        std::stringstream lineStream;
        std::getline(istream, line);
        lineStream << line;

        std::string info;
        lineStream >> info;

        if (info == "char")
        {
            // char id
            uint32_t charid = nextValuePair(&lineStream);
            // Char properties
            m_bmpChars[charid].x = nextValuePair(&lineStream);
            m_bmpChars[charid].y = nextValuePair(&lineStream);
            m_bmpChars[charid].width = nextValuePair(&lineStream);
            m_bmpChars[charid].height = nextValuePair(&lineStream);
            m_bmpChars[charid].xoffset = nextValuePair(&lineStream);
            m_bmpChars[charid].yoffset = nextValuePair(&lineStream);
            m_bmpChars[charid].xadvance = nextValuePair(&lineStream);
            m_bmpChars[charid].page = nextValuePair(&lineStream);
        }
    }
}


void DistanceFieldFonts::generateText(std::string text)
{
    std::vector<FontVertex> vertexs;
    std::vector<uint32_t> indexs;
    uint32_t indexOffset = 0;

    float w = m_pFontSdf->m_width;

    float posx = 0.0f;
    float posy = 0.0f;

    for (uint32_t i = 0; i < text.size(); i++)
    {
        BmpChar *charInfo = &m_bmpChars[(int)text[i]];

        if (charInfo->width == 0)
            charInfo->width = 36;

        float charw = ((float)(charInfo->width) / 36.0f);
        float dimx = 1.0f * charw;
        float charh = ((float)(charInfo->height) / 36.0f);
        float dimy = 1.0f * charh;

        float us = charInfo->x / w;
        float ue = (charInfo->x + charInfo->width) / w;
        float ts = charInfo->y / w;
        float te = (charInfo->y + charInfo->height) / w;

        float xo = charInfo->xoffset / 36.0f;
        float yo = charInfo->yoffset / 36.0f;

        posy = yo;

        vertexs.push_back({ { posx + dimx + xo,  posy + dimy, 0.0f }, { ue, te } });
        vertexs.push_back({ { posx + xo,         posy + dimy, 0.0f }, { us, te } });
        vertexs.push_back({ { posx + xo,         posy,        0.0f }, { us, ts } });
        vertexs.push_back({ { posx + dimx + xo,  posy,        0.0f }, { ue, ts } });

        std::array<uint32_t, 6> letterIndices = { 0,1,2, 2,3,0 };
        for (auto& index : letterIndices)
        {
            indexs.push_back(indexOffset + index);
        }
        indexOffset += 4;

        float advance = ((float)(charInfo->xadvance) / 36.0f);
        posx += advance;
    }
    
    // Center
    for (auto& v : vertexs)
    {
        v.pos[0] -= posx / 2.0f;
        v.pos[1] -= 0.5f;
    }

    m_indexCount = static_cast<uint32_t>(indexs.size());
    
    VkDeviceSize vertexSize = vertexs.size() * sizeof(FontVertex);
    VkDeviceSize indexSize = indexs.size() * sizeof(uint32_t);
    
    VkBuffer vertexStageBuffer;
    VkDeviceMemory vertexStageMemory;
    Tools::createBufferAndMemoryThenBind(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         vertexStageBuffer, vertexStageMemory);
    Tools::mapMemory(vertexStageMemory, vertexSize, vertexs.data());
    Tools::createBufferAndMemoryThenBind(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexMemory);
    
    VkBuffer indexStageBuffer;
    VkDeviceMemory indexStageMemory;
    Tools::createBufferAndMemoryThenBind(indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         indexStageBuffer, indexStageMemory);
    Tools::mapMemory(indexStageMemory, indexSize, indexs.data());
    Tools::createBufferAndMemoryThenBind(indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexMemory);
    

    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    VkBufferCopy vertexCopy = {};
    vertexCopy.srcOffset = 0;
    vertexCopy.dstOffset = 0;
    vertexCopy.size = vertexSize;
    vkCmdCopyBuffer(cmd, vertexStageBuffer, m_vertexBuffer, 1, &vertexCopy);
    
    VkBufferCopy indexCopy = {};
    indexCopy.srcOffset = 0;
    indexCopy.dstOffset = 0;
    indexCopy.size = indexSize;
    vkCmdCopyBuffer(cmd, indexStageBuffer, m_indexBuffer, 1, &indexCopy);
    
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    
    vkFreeMemory(m_device, vertexStageMemory, nullptr);
    vkDestroyBuffer(m_device, vertexStageBuffer, nullptr);
    vkFreeMemory(m_device, indexStageMemory, nullptr);
    vkDestroyBuffer(m_device, indexStageBuffer, nullptr);
}

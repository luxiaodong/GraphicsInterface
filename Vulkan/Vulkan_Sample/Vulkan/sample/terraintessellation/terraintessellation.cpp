
#include "terraintessellation.h"

TerrainHeight::TerrainHeight(std::string filename, uint32_t tileSize)
{
    ktxTexture* ktxTexture;
    ktxResult result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
    assert(result == KTX_SUCCESS);
    
    ktx_size_t ktxSize = ktxTexture_GetImageSize(ktxTexture, 0);
    ktx_uint8_t* ktxImage = ktxTexture_GetData(ktxTexture);
    m_length = ktxTexture->baseWidth;
    m_pData = new uint16_t[m_length * m_length];
    memcpy(m_pData, ktxImage, ktxSize);
    m_tileCount = m_length / tileSize;
    ktxTexture_Destroy(ktxTexture);
}

TerrainHeight::~TerrainHeight()
{
    delete[] m_pData;
}

float TerrainHeight::getHeight(uint32_t x, uint32_t y)
{
    glm::ivec2 pos = glm::ivec2(x, y) * glm::ivec2(m_tileCount);
    pos.x = std::max(0, std::min(pos.x, (int)m_length-1));
    pos.y = std::max(0, std::min(pos.y, (int)m_length-1));
    pos /= glm::ivec2(m_tileCount);
    
    uint32_t index = (pos.x + pos.y * m_length) * m_tileCount;
    return *(m_pData + index) / 65535.0f;
}


TerrainTessellation::TerrainTessellation(std::string title) : Application(title)
{
}

TerrainTessellation::~TerrainTessellation()
{}

void TerrainTessellation::init()
{
    Application::init();
    
    createTerrain();
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void TerrainTessellation::initCamera()
{
    m_camera.m_isFirstPersion = true;
//    m_camera.setPosition(glm::vec3(18.0f, 22.5f, 57.5f));
    m_camera.setTranslation(glm::vec3(0.4f, 1.25f, 0.0f));
    m_camera.setRotation(glm::vec3(-12.0f, 159.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void TerrainTessellation::setEnabledFeatures()
{
    // tessellation shader support is required for this example
    if(m_deviceFeatures.tessellationShader)
    {
        m_deviceEnabledFeatures.tessellationShader = VK_TRUE;
    }
    else
    {
         throw std::runtime_error("Selected GPU does not support tessellation shaders!");
    }
    
    if(m_deviceFeatures.fillModeNonSolid)
    {
        m_deviceEnabledFeatures.fillModeNonSolid = VK_TRUE;
    }
}

void TerrainTessellation::clear()
{
    vkDestroyPipeline(m_device, m_skyboxPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_skyboxPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_skyboxDescriptorSetLayout, nullptr);
    vkFreeMemory(m_device, m_skyboxUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_skyboxUniformBuffer, nullptr);
    
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_tessEvalMemory, nullptr);
    vkDestroyBuffer(m_device, m_tessEvalmBuffer, nullptr);
    
    vkFreeMemory(m_device, m_vertexMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexMemory, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);


    m_pSkyboxMap->clear();
    m_pTerrainMap->clear();
    m_pHeightMap->clear();
    delete m_pSkyboxMap;
    delete m_pTerrainMap;
    delete m_pHeightMap;
    m_skyboxLoader.clear();
    Application::clear();
}

void TerrainTessellation::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    m_skyboxLoader.loadFromFile(Tools::getModelPath() + "sphere.gltf", m_graphicsQueue, flags);
    m_skyboxLoader.createVertexAndIndexBuffer();
    m_skyboxLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal,  VertexComponent::UV});

    m_pSkyboxMap = Texture::loadTextrue2D(Tools::getTexturePath() + "skysphere_rgba.ktx", m_graphicsQueue);
    m_pTerrainMap = Texture::loadTextrue2D(Tools::getTexturePath() + "terrain_texturearray_rgba.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion::Layer);
    m_pHeightMap = Texture::loadTextrue2D(Tools::getTexturePath() + "terrain_heightmap_r16.ktx", m_graphicsQueue, VK_FORMAT_R16_UNORM);
}

void TerrainTessellation::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(SkyboxUniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_skyboxUniformBuffer, m_skyboxUniformMemory);
    
    SkyboxUniform mvp = {};
    // mvp.mvp = m_camera.m_projMat * glm::mat4(glm::mat3(m_camera.m_viewMat));
    mvp.mvp = m_camera.m_projMat * m_camera.m_viewMat;
    Tools::mapMemory(m_skyboxUniformMemory, uniformSize, &mvp);
    
    uniformSize = sizeof(TessEval);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_tessEvalmBuffer, m_tessEvalMemory);
    
    TessEval eval = {};
    eval.projection = m_camera.m_projMat;
    eval.modelview = m_camera.m_viewMat;
    eval.lightPos = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    eval.lightPos.y = -0.5f - eval.displacementFactor;
    eval.viewportDim = glm::vec2((float)m_swapchainExtent.width, (float)m_swapchainExtent.height);
    
    Frustum frustum;
    frustum.update(m_camera.m_projMat * m_camera.m_viewMat);
    memcpy(eval.frustumPlanes, frustum.m_planes.data(), sizeof(glm::vec4)*6);
    
    Tools::mapMemory(m_tessEvalMemory, uniformSize, &eval);
}

void TerrainTessellation::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_skyboxDescriptorSetLayout));
        createPipelineLayout(&m_skyboxDescriptorSetLayout, 1, m_skyboxPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout();
    }
}

void TerrainTessellation::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 3;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    
    {
        createDescriptorSet(&m_skyboxDescriptorSetLayout, 1, m_skyboxDescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SkyboxUniform);
        bufferInfo.buffer = m_skyboxUniformBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pSkyboxMap->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(TessEval);
        bufferInfo.buffer = m_tessEvalmBuffer;
        
        VkDescriptorImageInfo imageInfo1 = m_pHeightMap->getDescriptorImageInfo();
        VkDescriptorImageInfo imageInfo2 = m_pTerrainMap->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo1);
        writes[2] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void TerrainTessellation::createGraphicsPipeline()
{
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindDes.size());
    vertexInput.pVertexBindingDescriptions = m_vertexInputBindDes.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttrDes.size());
    vertexInput.pVertexAttributeDescriptions = m_vertexInputAttrDes.data();
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = Tools::getPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE);
    
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
    
    VkPipelineTessellationStateCreateInfo tessellation = {};
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation.patchControlPoints = 4;
    
    VkPipelineDynamicStateCreateInfo dynamic = Tools::getPipelineDynamicStateCreateInfo(dynamicStates);
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages;
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
    createInfo.pTessellationState = &tessellation;

    // object
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "terraintessellation/terrain.vert.spv");
    VkShaderModule tescModule = Tools::createShaderModule( Tools::getShaderPath() + "terraintessellation/terrain.tesc.spv");
    VkShaderModule teseModule = Tools::createShaderModule( Tools::getShaderPath() + "terraintessellation/terrain.tese.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "terraintessellation/terrain.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(tescModule, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shaderStages[2] = Tools::getPipelineShaderStageCreateInfo(teseModule, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    shaderStages[3] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, tescModule, nullptr);
    vkDestroyShaderModule(m_device, teseModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // skybox
    createInfo.pVertexInputState = m_skyboxLoader.getPipelineVertexInputState();
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    depthStencil.depthWriteEnable = VK_FALSE;
    createInfo.pTessellationState = nullptr;
    createInfo.layout = m_skyboxPipelineLayout;
    createInfo.stageCount = 2;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "terraintessellation/skysphere.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "terraintessellation/skysphere.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_skyboxPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void TerrainTessellation::updateRenderData()
{
}

void TerrainTessellation::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);
    m_skyboxLoader.bindBuffers(commandBuffer);
    m_skyboxLoader.draw(commandBuffer);
    
/*
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_terrainIndexCount, 1, 0, 0, 0);
*/
}

void TerrainTessellation::createTerrain()
{
    const uint32_t tileSize = 64;
    const uint32_t vertexCount = tileSize * tileSize;
    std::array<TerrainVertex, vertexCount> vertexs = {};
    
    TerrainHeight terrain(Tools::getTexturePath() + "terrain_heightmap_r16.ktx", tileSize);
    
    for(int y = 0; y < tileSize; ++y)
    {
        for(int x = 0; x < tileSize; ++x)
        {
            TerrainVertex v;
            v.pos.x = x * 2.0f + 1 - (float)tileSize;
            v.pos.y = 0.0f;
            v.pos.z = y * 2.0f + 1 - (float)tileSize;
            v.uv = glm::vec2((float)x / tileSize, (float)y / tileSize);
            
            float heights[3][3];
            for (auto hx = -1; hx <= 1; hx++)
            {
                for (auto hy = -1; hy <= 1; hy++)
                {
                    heights[hx+1][hy+1] = terrain.getHeight(x + hx, y + hy);
                }
            }
            
            glm::vec3 normal;
            normal.x = heights[0][0] - heights[2][0] + 2.0f * heights[0][1] - 2.0f * heights[2][1] + heights[0][2] - heights[2][2];
            normal.z = heights[0][0] + 2.0f * heights[1][0] + heights[2][0] - heights[0][2] - 2.0f * heights[1][2] - heights[2][2];
            normal.y = 0.25f * sqrt( 1.0f - normal.x * normal.x - normal.z * normal.z);
            v.normal = glm::normalize(normal * glm::vec3(2.0f, 1.0f, 2.0f));
            
            uint32_t index = x + y * tileSize;
            vertexs[index] = v;
        }
    }
    
    //indexs
    const uint32_t faceCount = (tileSize - 1) * (tileSize -  1) * 4;
    std::array<uint32_t, faceCount> indexs = {};
    
    for(int y = 0; y < tileSize - 1; ++y)
    {
        for(int x = 0; x < tileSize - 1; ++x)
        {
            uint32_t index = (x + y * (tileSize - 1)) * 4;
            indexs[index]     = x     + y     * tileSize;
            indexs[index + 1] = x     + (y+1) * tileSize;
            indexs[index + 2] = (x+1) + (y+1) * tileSize;
            indexs[index + 3] = (x+1) + y     * tileSize;
        }
    }
    
    m_terrainIndexCount = faceCount;
    
    //buffer
    m_vertexInputBindDes.clear();
    m_vertexInputBindDes.push_back(Tools::getVertexInputBindingDescription(0, sizeof(TerrainVertex)));
    
    m_vertexInputAttrDes.clear();
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertex, pos)));
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertex, normal)));
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(TerrainVertex, uv)));

    VkDeviceSize vertexSize = vertexs.size() * sizeof(TerrainVertex);
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

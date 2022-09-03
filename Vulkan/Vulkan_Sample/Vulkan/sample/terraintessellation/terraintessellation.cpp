
#include "terraintessellation.h"

TerrainTessellation::TerrainTessellation(std::string title) : Application(title)
{
}

TerrainTessellation::~TerrainTessellation()
{}

void TerrainTessellation::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void TerrainTessellation::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
    m_camera.setRotation(glm::vec3(-12.0f, 159.0f, 0.0f));
    m_camera.setTranslation(glm::vec3(18.0f, 22.5f, 57.5f));
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
    vkFreeMemory(m_device, m_tessControlMemory, nullptr);
    vkDestroyBuffer(m_device, m_tessControlmBuffer, nullptr);

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
    eval.modelView = m_camera.m_viewMat;
    eval.lightPos = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    eval.tessAlpha = 1.0f;
    eval.tessStrength = 0.1f;
    eval.lightPos.y = -0.5f - eval.tessStrength;
    
    Tools::mapMemory(m_tessEvalMemory, uniformSize, &eval);
    
    
    uniformSize = sizeof(TessControl);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_tessControlmBuffer, m_tessControlMemory);
    TessControl control = {};
    control.tessLevel = 64.0f;
    Tools::mapMemory(m_tessControlMemory, uniformSize, &control);
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
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout();
    }
}

void TerrainTessellation::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 3;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    
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
            
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = sizeof(TessControl);
        bufferInfo1.buffer = m_tessControlmBuffer;
        
        VkDescriptorBufferInfo bufferInfo2 = {};
        bufferInfo2.offset = 0;
        bufferInfo2.range = sizeof(TessEval);
        bufferInfo2.buffer = m_tessEvalmBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pHeightMap->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &bufferInfo2);
        writes[2] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void TerrainTessellation::createGraphicsPipeline()
{
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
    tessellation.patchControlPoints = 3;
    
    VkPipelineDynamicStateCreateInfo dynamic = Tools::getPipelineDynamicStateCreateInfo(dynamicStates);
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    
    createInfo.pVertexInputState = m_skyboxLoader.getPipelineVertexInputState();
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
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "displacement/base.vert.spv");
    VkShaderModule tescModule = Tools::createShaderModule( Tools::getShaderPath() + "displacement/displacement.tesc.spv");
    VkShaderModule teseModule = Tools::createShaderModule( Tools::getShaderPath() + "displacement/displacement.tese.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "displacement/base.frag.spv");
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
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    depthStencil.depthWriteEnable = VK_FALSE;
    createInfo.pTessellationState = nullptr;
    createInfo.stageCount = 2;
    createInfo.layout = m_skyboxPipelineLayout;
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
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width*0.5f, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);
    m_skyboxLoader.bindBuffers(commandBuffer);
    m_skyboxLoader.draw(commandBuffer);
    
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
//    m_objectLoader.bindBuffers(commandBuffer);
//    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
//    m_objectLoader.draw(commandBuffer);
}

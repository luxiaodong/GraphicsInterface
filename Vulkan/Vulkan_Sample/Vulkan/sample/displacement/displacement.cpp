
#include "displacement.h"

Displacement::Displacement(std::string title) : Application(title)
{
}

Displacement::~Displacement()
{}

void Displacement::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void Displacement::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -1.0f));
    m_camera.setRotation(glm::vec3(0.0f, -25.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void Displacement::setEnabledFeatures()
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

void Displacement::clear()
{
    vkDestroyPipeline(m_device, m_wireframePipeline, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_tessEvalMemory, nullptr);
    vkDestroyBuffer(m_device, m_tessEvalmBuffer, nullptr);
    vkFreeMemory(m_device, m_tessControlMemory, nullptr);
    vkDestroyBuffer(m_device, m_tessControlmBuffer, nullptr);

    m_pHeightMap->clear();
    delete m_pHeightMap;
    m_objectLoader.clear();
    Application::clear();
}

void Displacement::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    m_objectLoader.loadFromFile(Tools::getModelPath() + "displacement_plane.gltf", m_graphicsQueue, flags);
    m_objectLoader.createVertexAndIndexBuffer();
    m_objectLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal,  VertexComponent::Color});

    m_pHeightMap = Texture::loadTextrue2D(Tools::getTexturePath() + "stonefloor03_color_height_rgba.ktx", m_graphicsQueue);
}

void Displacement::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(TessEval);
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

void Displacement::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 1);
    bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 2);
    
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void Displacement::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);
    
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

void Displacement::createGraphicsPipeline()
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
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    
    createInfo.pVertexInputState = m_objectLoader.getPipelineVertexInputState();
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
    
    // wireframe
    rasterization.polygonMode = VK_POLYGON_MODE_LINE;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_wireframePipeline));
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, tescModule, nullptr);
    vkDestroyShaderModule(m_device, teseModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void Displacement::updateRenderData()
{
}

void Displacement::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width*0.5f, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_wireframePipeline);
    m_objectLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_objectLoader.draw(commandBuffer);
    
    viewport.x = viewport.width;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_objectLoader.draw(commandBuffer);
}

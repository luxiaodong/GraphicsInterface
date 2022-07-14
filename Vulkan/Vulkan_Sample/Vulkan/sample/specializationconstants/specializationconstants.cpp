
#include "specializationconstants.h"

SpecializationConstants::SpecializationConstants(std::string title) : Application(title)
{

}

SpecializationConstants::~SpecializationConstants()
{}

void SpecializationConstants::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void SpecializationConstants::initCamera()
{
    m_camera.setTranslation(glm::vec3(0.0f, 0.0f, -2.0f));
    m_camera.setRotation(glm::vec3(-40.0f, -90.0f, 0.0f));
    m_camera.setRotationSpeed(0.5f);
    m_camera.setPerspective(60.0f, (float)(m_width/3.0f) / (float)m_height, 0.1f, 512.0f);
}

//void SpecializationConstants::setEnabledFeatures()
//{
//    if( m_deviceFeatures.fillModeNonSolid )
//    {
//        m_deviceEnabledFeatures.fillModeNonSolid = VK_TRUE;
//        if( m_deviceFeatures.wideLines )
//        {
//            m_deviceEnabledFeatures.wideLines = VK_TRUE;
//        }
//    }
//}

void SpecializationConstants::clear()
{
    vkDestroyPipeline(m_device, m_phong, nullptr);
    vkDestroyPipeline(m_device, m_toon, nullptr);
    vkDestroyPipeline(m_device, m_textured, nullptr);
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    
    if(m_colorMap)
    {
        m_colorMap->clear();
        delete m_colorMap;
    }
    
    m_gltfLoader.clear();
    Application::clear();
}

void SpecializationConstants::prepareVertex()
{
    m_gltfLoader.loadFromFile(Tools::getModelPath() + "color_teapot_spheres.gltf", m_graphicsQueue);
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV, VertexComponent::Color});
    m_colorMap = Texture::loadTextrue2D(Tools::getTexturePath() +  "metalplate_nomips_rgba.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM);
}

void SpecializationConstants::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);

    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    
    Uniform mvp = {};
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.lightPos = glm::vec4(0.0f, -2.0f, 1.0f, 0.0f);
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void SpecializationConstants::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void SpecializationConstants::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);
    createDescriptorSet(m_descriptorSet);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    bufferInfo.buffer = m_uniformBuffer;
    
    VkDescriptorImageInfo imageInfo = m_colorMap->getDescriptorImageInfo();

    std::array<VkWriteDescriptorSet, 2> writes = {};
    writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
    writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void SpecializationConstants::prepareSpecializationInfo()
{
    // Each shader constant of a shader stage corresponds to one map entry
    // Shader bindings based on specialization constants are marked by the new "constant_id" layout qualifier:
    //    layout (constant_id = 0) const int LIGHTING_MODEL = 0;
    //    layout (constant_id = 1) const float PARAM_TOON_DESATURATION = 0.0f;

    // Map entry for the lighting model to be used by the fragment shader
    m_specializationMapEntries[0].constantID = 0;
    m_specializationMapEntries[0].size = sizeof(m_specializationData.lightingModel);
    m_specializationMapEntries[0].offset = offsetof(SpecializationData, lightingModel);

    // Map entry for the toon shader parameter
    m_specializationMapEntries[1].constantID = 1;
    m_specializationMapEntries[1].size = sizeof(m_specializationData.toonDesaturationFactor);
    m_specializationMapEntries[1].offset = offsetof(SpecializationData, toonDesaturationFactor);

    // Prepare specialization info block for the shader stage
    m_specializationInfo.dataSize = sizeof(SpecializationData);
    m_specializationInfo.mapEntryCount = static_cast<uint32_t>(m_specializationMapEntries.size());
    m_specializationInfo.pMapEntries = m_specializationMapEntries.data();
    m_specializationInfo.pData = &m_specializationData;
}

void SpecializationConstants::createGraphicsPipeline()
{
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "specializationconstants/uber.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "specializationconstants/uber.frag.spv");
    
    VkPipelineShaderStageCreateInfo vertShader = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    VkPipelineShaderStageCreateInfo fragShader = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

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
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    VkPipelineDynamicStateCreateInfo dynamic = Tools::getPipelineDynamicStateCreateInfo(dynamicStates);
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vertShader);
    shaderStages.push_back(fragShader);
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(shaderStages, m_pipelineLayout, m_renderPass);

    createInfo.pVertexInputState = m_gltfLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    prepareSpecializationInfo();
    // Specialization info is assigned is part of the shader stage (modul) and must be set after creating the module and before creating the pipeline
    shaderStages[1].pSpecializationInfo = &m_specializationInfo;

    m_specializationData.lightingModel = 0;
    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_phong) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    
    m_specializationData.lightingModel = 1;
    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_toon) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    
    m_specializationData.lightingModel = 2;
    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_textured) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void SpecializationConstants::updateRenderData()
{
    
}

void SpecializationConstants::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_gltfLoader.bindBuffers(commandBuffer);

    // phong
    viewport.width = (float)m_swapchainExtent.width/3.0;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_phong);
    m_gltfLoader.draw(commandBuffer);

    // toon
    viewport.x = (float)m_swapchainExtent.width/3.0;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_toon);
    m_gltfLoader.draw(commandBuffer);

    // textured
    viewport.x += (float)m_swapchainExtent.width/3.0;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textured);
    m_gltfLoader.draw(commandBuffer);
}

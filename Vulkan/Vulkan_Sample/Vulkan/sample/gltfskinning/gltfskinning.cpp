
#include "gltfskinning.h"

GltfSkinning::GltfSkinning(std::string title) : Application(title)
{
}

GltfSkinning::~GltfSkinning()
{}

void GltfSkinning::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void GltfSkinning::initCamera()
{
    m_camera.m_isFlipY = true;
    m_camera.setPosition(glm::vec3(0.0f, 0.75f, -2.0f));
    m_camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    m_camera.setRotationSpeed(0.5f);
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void GltfSkinning::setEnabledFeatures()
{
}

void GltfSkinning::clear()
{
    vkDestroyDescriptorSetLayout(m_device, m_jointMatrixDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_textureDescriptorSetLayout, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);

    m_gltfLoader.clear();
    Application::clear();
}

void GltfSkinning::prepareVertex()
{
    m_gltfLoader.loadFromFile(Tools::getModelPath() + "CesiumMan/glTF/CesiumMan.gltf", m_graphicsQueue, GltfFileLoadFlags::None);
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV, VertexComponent::Color, VertexComponent::JointIndex, VertexComponent::JointWeight});
}

void GltfSkinning::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    Uniform mvp = {};
    mvp.viewMatrix = m_camera.m_viewMat * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));;
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
}

void GltfSkinning::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(&binding, 1);
    
    VkDescriptorSetLayoutBinding binding1 = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    VkDescriptorSetLayoutCreateInfo createInfo1 = Tools::getDescriptorSetLayoutCreateInfo(&binding1, 1);
    VK_CHECK_RESULT( vkCreateDescriptorSetLayout(m_device, &createInfo1, nullptr, &m_jointMatrixDescriptorSetLayout) );
    
    VkDescriptorSetLayoutBinding binding2 = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VkDescriptorSetLayoutCreateInfo createInfo2 = Tools::getDescriptorSetLayoutCreateInfo(&binding2, 1);
    VK_CHECK_RESULT( vkCreateDescriptorSetLayout(m_device, &createInfo2, nullptr, &m_textureDescriptorSetLayout) );
    
    VkDescriptorSetLayout descriptorSetLayout[3] = {m_descriptorSetLayout, m_jointMatrixDescriptorSetLayout, m_textureDescriptorSetLayout};
    
    VkPushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);
    
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.setLayoutCount = 3;
    createInfo.pSetLayouts = descriptorSetLayout;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK_RESULT( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) );
}

void GltfSkinning::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 3> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(m_gltfLoader.m_textures.size());
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = 1;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), static_cast<uint32_t>(m_gltfLoader.m_textures.size()) + 2);
    
    {
        createDescriptorSet(m_descriptorSet);

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
    
    {
        createDescriptorSet(&m_jointMatrixDescriptorSetLayout, 1,  m_gltfLoader.m_skins.at(0)->m_descriptorSet);

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = m_gltfLoader.m_skins.at(0)->m_totalSize;
        bufferInfo.buffer = m_gltfLoader.m_skins.at(0)->m_jointMatrixBuffer;
        
        VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_gltfLoader.m_skins.at(0)->m_descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
    
    {
        for(Texture* pTex : m_gltfLoader.m_textures)
        {
            createDescriptorSet(&m_textureDescriptorSetLayout, 1, pTex->m_descriptorSet);

            VkDescriptorImageInfo imageInfo = pTex->getDescriptorImageInfo();
            VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(pTex->m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo);
            vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
        }
    }
}

void GltfSkinning::createGraphicsPipeline()
{
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

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

    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "gltfskinning/skinnedmodel.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "gltfskinning/skinnedmodel.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void GltfSkinning::updateRenderData()
{
    m_gltfLoader.updateAnimation(1.0f);
}

void GltfSkinning::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_gltfLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_gltfLoader.draw(commandBuffer, m_pipelineLayout, 2);
}

std::vector<VkClearValue> GltfSkinning::getClearValue()
{
    std::vector<VkClearValue> clearValues = {};
    VkClearValue color = {};
    color.color = {{0.25f, 0.25f, 0.25f, 1.0f}};
    VkClearValue depth = {};
    depth.depthStencil = {1.0f, 0};
    
    clearValues.push_back(color);
    clearValues.push_back(depth);
    return clearValues;
}

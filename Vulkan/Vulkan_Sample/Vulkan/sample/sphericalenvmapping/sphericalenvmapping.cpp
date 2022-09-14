
#include "sphericalenvmapping.h"

SphericalEnvMapping::SphericalEnvMapping(std::string title) : Application(title)
{
}

SphericalEnvMapping::~SphericalEnvMapping()
{}

void SphericalEnvMapping::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();

    createGraphicsPipeline();
}

void SphericalEnvMapping::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -3.5f));
    m_camera.setRotation(glm::vec3(-25.0f, 23.75f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void SphericalEnvMapping::setEnabledFeatures()
{
}

void SphericalEnvMapping::clear()
{
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);

    m_dragonLoader.clear();
    m_pTexture->clear();
    delete m_pTexture;
    Application::clear();
}

void SphericalEnvMapping::prepareVertex()
{
    uint32_t loadFlags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    m_dragonLoader.loadFromFile(Tools::getModelPath() + "chinesedragon.gltf", m_graphicsQueue, loadFlags);
    m_dragonLoader.createVertexAndIndexBuffer();
    m_dragonLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position,VertexComponent::Normal,VertexComponent::Color});
    
    m_pTexture = Texture::loadTextrue2D(Tools::getTexturePath() +  "matcap_array_rgba.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion::Layer);
}

void SphericalEnvMapping::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);

    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::mat4(1.0f);
//    mvp.normal = glm::inverseTranspose(mvp.viewMatrix * mvp.modelMatrix);
    mvp.normal = glm::inverse(glm::transpose(mvp.viewMatrix * mvp.modelMatrix));
//    mvp.normal = glm::transpose(glm::inverse(mvp.viewMatrix * mvp.modelMatrix));
    
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
}

void SphericalEnvMapping::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    createDescriptorSetLayout(bindings, 2);
    createPipelineLayout();
}

void SphericalEnvMapping::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);
    
    {
        createDescriptorSet(m_descriptorSet);
            
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo1 = m_pTexture->getDescriptorImageInfo();
        
        VkWriteDescriptorSet writes[2] = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo1);
        vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
    }
}

void SphericalEnvMapping::createGraphicsPipeline()
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, 0xf);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    
    createInfo.pVertexInputState = m_dragonLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "sphericalenvmapping/sem.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "sphericalenvmapping/sem.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void SphericalEnvMapping::updateRenderData()
{
}

void SphericalEnvMapping::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_dragonLoader.bindBuffers(commandBuffer);
    m_dragonLoader.draw(commandBuffer);
}

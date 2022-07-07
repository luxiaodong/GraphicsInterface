
#include "pipelines.h"

Pipelines::Pipelines(std::string title) : Application(title)
{
    m_pModel = new vkglTF::Model();
}

Pipelines::~Pipelines()
{}

void Pipelines::init()
{
    Application::init();

    createDescriptorSetLayout();
    createPipelineLayout();
    
    loadModel();
//    prepareVertex();
    prepareUniform();
    createDescriptorSet();
    createGraphicsPipeline();
}

void Pipelines::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
    m_camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
    m_camera.setRotationSpeed(0.5f);
    m_camera.setPerspective(60.0f, (float)(m_width / 3.0f) / (float)m_height, 0.1f, 256.0f);
}

void Pipelines::setEnabledFeatures()
{
    if( m_deviceFeatures.fillModeNonSolid )
    {
        m_deviceEnabledFeatures.fillModeNonSolid = VK_TRUE;
        if( m_deviceFeatures.wideLines )
        {
            m_deviceEnabledFeatures.wideLines = VK_TRUE;
        }
    }
}

void Pipelines::clear()
{
    delete m_pModel;
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);

    vkDestroyPipeline(m_device, m_phong, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

    Application::clear();
}

void Pipelines::loadModel()
{
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
    
    m_pModel->loadFromFile(Tools::getModelPath() + "treasure_smooth.gltf", m_graphicsQueue, glTFLoadingFlags);
}

void Pipelines::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Pipelines::Uniform);

    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    
    Pipelines::Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.modelMatrix = m_camera.m_viewMat;
    Tools::mapMemory(m_uniformMemory, sizeof(Pipelines::Uniform), &mvp);
}

void Pipelines::createDescriptorSet()
{
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;
    std::vector<VkDescriptorPoolSize> poolSizes = {poolSize};
    createDescriptorPool(poolSizes, 2);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    if( vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptorSets!");
    }

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Pipelines::Uniform);
    bufferInfo.buffer = m_uniformBuffer;

    VkWriteDescriptorSet writeSetBuffer = {};
    writeSetBuffer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSetBuffer.dstSet = m_descriptorSet;
    writeSetBuffer.dstBinding = 0;
    writeSetBuffer.dstArrayElement = 0;
    writeSetBuffer.descriptorCount = 1;
    writeSetBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeSetBuffer.pImageInfo = nullptr;
    writeSetBuffer.pBufferInfo = &bufferInfo;
    writeSetBuffer.pTexelBufferView = nullptr;
    vkUpdateDescriptorSets(m_device, 1, &writeSetBuffer, 0, nullptr);
}

void Pipelines::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
    
    if ( vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create descriptorSetLayout!");
    }
}

void Pipelines::createPipelineLayout()
{
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &m_descriptorSetLayout;
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges = nullptr;

    if( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create layout!");
    }
}

void Pipelines::createGraphicsPipeline()
{
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "pipelines/phong.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "pipelines/phong.frag.spv");

    VkPipelineShaderStageCreateInfo vertShader = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    VkPipelineShaderStageCreateInfo fragShader = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

//    VkPipelineVertexInputStateCreateInfo vertexInput = {};
//    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindDes.size());
//    vertexInput.pVertexBindingDescriptions = m_vertexInputBindDes.data();
//    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttrDes.size());
//    vertexInput.pVertexAttributeDescriptions = m_vertexInputAttrDes.data();

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
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vertShader);
    shaderStages.push_back(fragShader);

    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(shaderStages, m_pipelineLayout, m_renderPass);

    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
//    createInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_phong) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void Pipelines::prepareRenderData()
{
//    static int i = 0;
//    i++;
//    Triangle::Uniform mvp = {};
//
//    mvp.modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(1.0f*i), glm::vec3(0.0f, 0.0f, 1.0f));
//    mvp.viewMatrix = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//    mvp.projectionMatrix = glm::perspective(glm::radians(45.0f), m_swapchainExtent.width / (float) m_swapchainExtent.height, 0.1f, 10.0f);
//    mvp.projectionMatrix[1][1] *= -1;
//
//    Tools::mapMemory(m_uniformMemory, sizeof(Triangle::Uniform), &mvp);
}

void Pipelines::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_phong);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

//    VkDeviceSize offset = {0};
//    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
//    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_pModel->bindBuffers(commandBuffer);
    m_pModel->draw(commandBuffer);
}

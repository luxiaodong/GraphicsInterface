
#include "descriptorsets.h"

Descriptorsets::Descriptorsets(std::string title) : Application(title)
{
}

Descriptorsets::~Descriptorsets()
{}

void Descriptorsets::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void Descriptorsets::initCamera()
{
    m_camera.setTranslation(glm::vec3(0.0f, 0.0f, -20.0f));
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 512.0f);
}

void Descriptorsets::setEnabledFeatures()
{
    if( m_deviceFeatures.samplerAnisotropy )
    {
        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
    }
}

void Descriptorsets::clear()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);

    for(int i = 0; i < 2; ++i)
    {
        vkFreeMemory(m_device, m_cube[i].uniformMemory, nullptr);
        vkDestroyBuffer(m_device, m_cube[i].uniformBuffer, nullptr);
        
        if(m_cube[i].pTextrue)
        {
            m_cube[i].pTextrue->clear();
            delete m_cube[i].pTextrue;
        }
    }
    
    m_gltfLoader.clear();
    Application::clear();
}

void Descriptorsets::prepareVertex()
{
    m_gltfLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue);
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV, VertexComponent::Color});
    m_cube[0].pTextrue = Texture::loadTextrue2D(Tools::getTexturePath() +  "crate01_color_height_rgba.ktx", m_graphicsQueue);
    m_cube[1].pTextrue = Texture::loadTextrue2D(Tools::getTexturePath() +  "crate02_color_height_rgba.ktx", m_graphicsQueue);
}

void Descriptorsets::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Descriptorsets::Uniform);
    
    for(int i = 0; i < 2; ++i)
    {
        Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            m_cube[i].uniformBuffer, m_cube[i].uniformMemory);
    }
}

void Descriptorsets::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void Descriptorsets::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);

    for(int i = 0; i < 2; ++i)
    {
        createDescriptorSet(m_cube[i].descriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Descriptorsets::Uniform);
        bufferInfo.buffer = m_cube[i].uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = m_cube[i].pTextrue->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_cube[i].descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_cube[i].descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void Descriptorsets::createGraphicsPipeline()
{
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "descriptorsets/cube.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "descriptorsets/cube.frag.spv");
    
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
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    
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
    
    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void Descriptorsets::updateRenderData()
{
    m_cube[0].rotation = glm::vec3(3.5f, 0.0f, 0.0f);
    m_cube[1].rotation = glm::vec3(0.0f, 5.0f, 0.0f);
    m_cube[0].matrix.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
    m_cube[1].matrix.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3( 1.5f, 0.5f, 0.0f));
    
    for(int i = 0; i < 2; ++i)
    {
        m_cube[i].matrix.viewMatrix = m_camera.m_viewMat;
        m_cube[i].matrix.projectionMatrix = m_camera.m_projMat;
        m_cube[i].matrix.modelMatrix = glm::rotate(m_cube[i].matrix.modelMatrix, glm::radians(m_cube[i].rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m_cube[i].matrix.modelMatrix = glm::rotate(m_cube[i].matrix.modelMatrix, glm::radians(m_cube[i].rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m_cube[i].matrix.modelMatrix = glm::rotate(m_cube[i].matrix.modelMatrix, glm::radians(m_cube[i].rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m_cube[i].matrix.modelMatrix = glm::scale(m_cube[i].matrix.modelMatrix, glm::vec3(0.2f));
        Tools::mapMemory(m_cube[i].uniformMemory, sizeof(Descriptorsets::Uniform), &m_cube[i].matrix);
    }
}

void Descriptorsets::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    m_gltfLoader.bindBuffers(commandBuffer);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    
    for(int i=0; i<2; ++i)
    {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_cube[i].descriptorSet, 0, nullptr);
        m_gltfLoader.draw(commandBuffer);
    }
}


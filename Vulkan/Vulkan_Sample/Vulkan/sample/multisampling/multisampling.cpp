
#include "multisampling.h"

MultiSampling::MultiSampling(std::string title) : Application(title)
{
}

MultiSampling::~MultiSampling()
{}

void MultiSampling::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void MultiSampling::initCamera()
{
    m_camera.setTranslation(glm::vec3(2.5f, 2.5f, -7.5f));
    m_camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)(m_width) / (float)m_height, 0.1f, 256.0f);
}

void MultiSampling::setEnabledFeatures()
{
    if(m_deviceFeatures.sampleRateShading)
    {
        m_deviceEnabledFeatures.sampleRateShading = VK_TRUE;
    }
    
    if(m_deviceFeatures.samplerAnisotropy)
    {
        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
    }
}

void MultiSampling::clear()
{
    vkDestroyDescriptorSetLayout(m_device, m_textureDescriptorSetLayout, nullptr);
    vkDestroyPipeline(m_device, m_multiSamplingPipeline, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    m_gltfLoader.clear();

    Application::clear();
}

void MultiSampling::prepareVertex()
{
    m_gltfLoader.loadFromFile(Tools::getModelPath() + "voyager.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::FlipY );
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV, VertexComponent::Color});
}

void MultiSampling::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);

    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.lightPos = glm::vec4(5.0f, -5.0f, 5.0f, 1.0f);
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void MultiSampling::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(&binding, 1);
    
    VkDescriptorSetLayoutBinding binding2 = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VkDescriptorSetLayoutCreateInfo createInfo1 = Tools::getDescriptorSetLayoutCreateInfo(&binding2, 1);
    
    VK_CHECK_RESULT( vkCreateDescriptorSetLayout(m_device, &createInfo1, nullptr, &m_textureDescriptorSetLayout) );
    
    VkDescriptorSetLayout descriptorSetLayout[2] = {m_descriptorSetLayout, m_textureDescriptorSetLayout};
    
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.setLayoutCount = 2;
    createInfo.pSetLayouts = descriptorSetLayout;
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges = nullptr;

    VK_CHECK_RESULT( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) );
}

void MultiSampling::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(m_gltfLoader.m_textures.size());
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), static_cast<uint32_t>(m_gltfLoader.m_textures.size()) + 1);
    
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
        for(Texture* pTex : m_gltfLoader.m_textures)
        {
            createDescriptorSet(&m_textureDescriptorSetLayout, 1, pTex->m_descriptorSet);

            VkDescriptorImageInfo imageInfo = pTex->getDescriptorImageInfo();
            VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(pTex->m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo);
            vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
        }
    }
}

void MultiSampling::createGraphicsPipeline()
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
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

//    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

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

    VkStencilOpState stencilOpState = {};
    depthStencil.stencilTestEnable = VK_TRUE;
    stencilOpState.compareOp = VK_COMPARE_OP_ALWAYS;
    stencilOpState.passOp = VK_STENCIL_OP_REPLACE;
    stencilOpState.failOp = VK_STENCIL_OP_REPLACE;
    stencilOpState.depthFailOp = VK_STENCIL_OP_REPLACE;
    stencilOpState.compareMask = 0xff;
    stencilOpState.writeMask = 0xff;
    stencilOpState.reference = 1;
    depthStencil.back = stencilOpState;
    depthStencil.front = stencilOpState;
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "multisampling/mesh.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "multisampling/mesh.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void MultiSampling::updateRenderData()
{
    
}

void MultiSampling::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_gltfLoader.bindBuffers(commandBuffer);
    
    // pass 1
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_gltfLoader.draw(commandBuffer, m_pipelineLayout, 1);
    
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_outlinePipeline);
//    m_gltfLoader.draw(commandBuffer);
}

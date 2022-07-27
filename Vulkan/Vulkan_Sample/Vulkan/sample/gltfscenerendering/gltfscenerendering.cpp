
#include "gltfscenerendering.h"

GltfSceneRendering::GltfSceneRendering(std::string title) : Application(title)
{
}

GltfSceneRendering::~GltfSceneRendering()
{}

void GltfSceneRendering::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void GltfSceneRendering::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.m_isFlipY = true;
    m_camera.setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
    m_camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
    m_camera.setRotationSpeed(0.5f);
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void GltfSceneRendering::setEnabledFeatures()
{
}

void GltfSceneRendering::clear()
{
//    vkDestroyPipelineLayout(m_device, m_textruePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_textureDescriptorSetLayout, nullptr);
//    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);

    m_gltfLoader.clear();
    Application::clear();
}

void GltfSceneRendering::prepareVertex()
{
    m_gltfLoader.loadFromFile(Tools::getModelPath() + "sponza/sponza.gltf", m_graphicsQueue, GltfFileLoadFlags::None);
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV, VertexComponent::Color, VertexComponent::Tangent});
}

void GltfSceneRendering::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    Uniform mvp = {};
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.lightPos = glm::vec4(0.0f, 2.5f, 0.0f, 1.0f);
    mvp.viewPos = m_camera.m_viewPos;
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
}

void GltfSceneRendering::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(&binding, 1);
    
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    VkDescriptorSetLayoutCreateInfo createInfo1 = Tools::getDescriptorSetLayoutCreateInfo(bindings, 2);
    
    VK_CHECK_RESULT( vkCreateDescriptorSetLayout(m_device, &createInfo1, nullptr, &m_textureDescriptorSetLayout) );
    
    VkDescriptorSetLayout descriptorSetLayout[2] = {m_descriptorSetLayout, m_textureDescriptorSetLayout};
    
    VkPushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.setLayoutCount = 2;
    createInfo.pSetLayouts = descriptorSetLayout;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK_RESULT( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) );
}

void GltfSceneRendering::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(m_gltfLoader.m_materials.size()) * 2;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), static_cast<uint32_t>(m_gltfLoader.m_materials.size()) + 1);
    
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
        uint32_t i = 0;
        std::cout << m_gltfLoader.m_materials.size() << std::endl;
        for(Material* mat : m_gltfLoader.m_materials)
        {
            createDescriptorSet(&m_textureDescriptorSetLayout, 1, mat->m_descriptorSet);

            VkDescriptorImageInfo imageInfo1 = mat->m_pBaseColorTexture->getDescriptorImageInfo();
            VkDescriptorImageInfo imageInfo2 = mat->m_pNormalTexture->getDescriptorImageInfo();
            
            VkWriteDescriptorSet writes[2] = {};
            writes[0] = Tools::getWriteDescriptorSet(mat->m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo1);
            writes[1] = Tools::getWriteDescriptorSet(mat->m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo2);
            vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
            i++;
        }
    }
}

void GltfSceneRendering::createGraphicsPipeline()
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

    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "gltfscenerendering/scene.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "gltfscenerendering/scene.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    
    for(Material* mat : m_gltfLoader.m_materials)
    {
        SpecializationData specializationData = {};
        std::array<VkSpecializationMapEntry, 2> specializationMapEntries;
        VkSpecializationInfo specializationInfo = {};
        
        specializationData.alphaMask = mat->m_alphaMode == Material::AlphaMode::MASK;
        specializationData.alphaMaskCutoff = mat->m_alphaCutoff;
        
        specializationMapEntries[0].constantID = 0;
        specializationMapEntries[0].size = sizeof(specializationData.alphaMask);
        specializationMapEntries[0].offset = offsetof(SpecializationData, alphaMask);

        // Map entry for the toon shader parameter
        specializationMapEntries[1].constantID = 1;
        specializationMapEntries[1].size = sizeof(specializationData.alphaMaskCutoff);
        specializationMapEntries[1].offset = offsetof(SpecializationData, alphaMaskCutoff);

        // Prepare specialization info block for the shader stage
        specializationInfo.dataSize = sizeof(SpecializationData);
        specializationInfo.mapEntryCount = static_cast<uint32_t>(specializationMapEntries.size());
        specializationInfo.pMapEntries = specializationMapEntries.data();
        specializationInfo.pData = &specializationData;
        
        shaderStages[1].pSpecializationInfo = &specializationInfo;
        
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &mat->m_graphicsPipeline));
        mat->m_isNeedVkBuffer = true;
    }
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void GltfSceneRendering::updateRenderData()
{
}

void GltfSceneRendering::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    m_gltfLoader.bindBuffers(commandBuffer);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_gltfLoader.draw(commandBuffer, m_pipelineLayout, 3);
    
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
//    m_gltfLoader.draw(commandBuffer, m_pipelineLayout, 1);
}

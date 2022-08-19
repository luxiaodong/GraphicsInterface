
#include "texturecubemapping.h"

TextureCubeMapping::TextureCubeMapping(std::string title) : Application(title)
{
}

TextureCubeMapping::~TextureCubeMapping()
{}

void TextureCubeMapping::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void TextureCubeMapping::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -4.0f));
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void TextureCubeMapping::setEnabledFeatures()
{
//    if(m_deviceFeatures.samplerAnisotropy)
//    {
//        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
//    }
}

void TextureCubeMapping::clear()
{
    vkDestroyPipeline(m_device, m_skyboxPipeline, nullptr);
    vkDestroyPipeline(m_device, m_objectPipeline, nullptr);

    vkFreeMemory(m_device, m_skyboxUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_skyboxUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_objectUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_objectUniformBuffer, nullptr);
    
    m_skyboxLoader.clear();
    m_objectLoader.clear();
    m_pTexture->clear();
    delete m_pTexture;
    Application::clear();
}

void TextureCubeMapping::prepareVertex()
{
    m_skyboxLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue);
    m_skyboxLoader.createVertexAndIndexBuffer();
    m_skyboxLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal});
    m_pTexture = Texture::loadTextrue2D(Tools::getTexturePath() +  "cubemap_yokohama_rgba.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion::Cube);
//    m_pTexture = Texture::loadTextrue2D(Tools::getTexturePath() +  "hdr/pisa_cube.ktx", m_graphicsQueue, VK_FORMAT_R16G16B16A16_SFLOAT, TextureCopyRegion::Cube);
    
    std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
    m_objectLoader.loadFromFile(Tools::getModelPath() + filenames[0], m_graphicsQueue);
    m_objectLoader.createVertexAndIndexBuffer();
    m_objectLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal});
}

void TextureCubeMapping::prepareUniform()
{
    // skybox
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(offsetof(Uniform, invViewMatrix), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_skyboxUniformBuffer, m_skyboxUniformMemory);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.invViewMatrix = glm::inverse(m_camera.m_viewMat);
    mvp.lodBias = 0.0f;
    Tools::mapMemory(m_skyboxUniformMemory, offsetof(Uniform, invViewMatrix), &mvp);
    
    // object
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_objectUniformBuffer, m_objectUniformMemory);
    Tools::mapMemory(m_objectUniformMemory, uniformSize, &mvp);
    
    // 3D object
//    uboVS.projection = camera.matrices.perspective;
//    uboVS.modelView = camera.matrices.view;
//    uboVS.inverseModelview = glm::inverse(camera.matrices.view);
//    memcpy(uniformBuffers.object.mapped, &uboVS, sizeof(uboVS));
//    // Skybox
//    uboVS.modelView = camera.matrices.view;
//    // Cancel out translation
//    uboVS.modelView[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
//    memcpy(uniformBuffers.skybox.mapped, &uboVS, sizeof(uboVS));
    
}

void TextureCubeMapping::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void TextureCubeMapping::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    createDescriptorSet(m_skyboxDescriptorSet);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    bufferInfo.buffer = m_skyboxUniformBuffer;
    
    VkDescriptorImageInfo imageInfo = m_pTexture->getDescriptorImageInfo();

    std::array<VkWriteDescriptorSet, 2> writes = {};
    writes[0] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
    writes[1] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
    //object
    createDescriptorSet(m_objectDescriptorSet);
    bufferInfo.buffer = m_objectUniformBuffer;
    writes[0] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
    writes[1] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void TextureCubeMapping::createGraphicsPipeline()
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

//    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

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
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "texturecubemap/skybox.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "texturecubemap/skybox.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_skyboxPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //object
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "texturecubemap/reflect.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "texturecubemap/reflect.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    createInfo.pVertexInputState = m_objectLoader.getPipelineVertexInputState();
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_objectPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void TextureCubeMapping::updateRenderData()
{
}

void TextureCubeMapping::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    m_skyboxLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);
    m_skyboxLoader.draw(commandBuffer);
    
    // object
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_objectPipeline);
    m_objectLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_objectDescriptorSet, 0, nullptr);
    m_objectLoader.draw(commandBuffer);
}


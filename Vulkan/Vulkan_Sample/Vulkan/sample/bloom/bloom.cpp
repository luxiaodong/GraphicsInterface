
#include "bloom.h"

Bloom::Bloom(std::string title) : Application(title)
{
}

Bloom::~Bloom()
{}

void Bloom::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createOffscreenRenderPass();
    createOffscreenFrameBuffer();
    
    createGraphicsPipeline();
}

void Bloom::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -10.25f));
    m_camera.setRotation(glm::vec3(7.5f, -343.0f, 0.0f));
    m_camera.setPerspective(45.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void Bloom::setEnabledFeatures()
{
//    if(m_deviceFeatures.samplerAnisotropy)
//    {
//        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
//    }
}

void Bloom::clear()
{
    vkDestroyPipeline(m_device, m_glowPipeline, nullptr);
    vkDestroyPipeline(m_device, m_skyboxPipeline, nullptr);
    vkDestroyPipeline(m_device, m_objectPipeline, nullptr);
    vkDestroyPipeline(m_device, m_bloomPipeline[0], nullptr);
    vkDestroyPipeline(m_device, m_bloomPipeline[1], nullptr);
    
//    vkDestroyDescriptorSetLayout(m_device, m_blurDescriptorSetLayout, nullptr);
//    vkDestroyPipelineLayout(m_device, m_blurPipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_offscreenFrameBuffer[0], nullptr);
    vkDestroyFramebuffer(m_device, m_offscreenFrameBuffer[1], nullptr);
    
    vkDestroyImageView(m_device, m_offscreenColorImageView[0], nullptr);
    vkDestroyImage(m_device, m_offscreenColorImage[0], nullptr);
    vkFreeMemory(m_device, m_offscreenColorMemory[0], nullptr);
    vkDestroyImageView(m_device, m_offscreenColorImageView[1], nullptr);
    vkDestroyImage(m_device, m_offscreenColorImage[1], nullptr);
    vkFreeMemory(m_device, m_offscreenColorMemory[1], nullptr);
    vkDestroySampler(m_device, m_offscreenColorSample, nullptr);
    vkDestroyImageView(m_device, m_offscreenDepthImageView[0], nullptr);
    vkDestroyImage(m_device, m_offscreenDepthImage[0], nullptr);
    vkFreeMemory(m_device, m_offscreenDepthMemory[0], nullptr);
    vkDestroyImageView(m_device, m_offscreenDepthImageView[1], nullptr);
    vkDestroyImage(m_device, m_offscreenDepthImage[1], nullptr);
    vkFreeMemory(m_device, m_offscreenDepthMemory[1], nullptr);

    vkFreeMemory(m_device, m_skyboxUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_skyboxUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_objectUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_objectUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_blurParamUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_blurParamUniformBuffer, nullptr);
    
    
    m_skyboxLoader.clear();
    m_objectLoader.clear();
    m_glowLoader.clear();
    m_pTexture->clear();
    delete m_pTexture;
    Application::clear();
}

void Bloom::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    
    m_skyboxLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue, flags);
    m_skyboxLoader.createVertexAndIndexBuffer();
    
    m_objectLoader.loadFromFile(Tools::getModelPath() + "retroufo.gltf", m_graphicsQueue, flags);
    m_objectLoader.createVertexAndIndexBuffer();
    
    m_glowLoader.loadFromFile(Tools::getModelPath() + "retroufo_glow.gltf", m_graphicsQueue, flags);
    m_glowLoader.createVertexAndIndexBuffer();
    
    m_skyboxLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal});
    
    m_pTexture = Texture::loadTextrue2D(Tools::getTexturePath() +  "cubemap_space.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion::Cube);
}

void Bloom::prepareUniform()
{
    // skybox
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_skyboxUniformBuffer, m_skyboxUniformMemory);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_objectUniformBuffer, m_objectUniformMemory);

    float timer = 0.0f;
    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat = glm::translate(modelMat, glm::vec3(sin(glm::radians(timer * 360.0f)) * 0.25f, -1.0f, cos(glm::radians(timer * 360.0f)) * 0.25f));
    modelMat = glm::rotate(modelMat, -sinf(glm::radians(timer * 360.0f)) * 0.15f, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMat = glm::rotate(modelMat, glm::radians(timer * 360.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = modelMat;
    Tools::mapMemory(m_objectUniformMemory, sizeof(Uniform), &mvp);
    
    mvp.projectionMatrix = glm::perspective(glm::radians(45.0f), (float)m_width / (float)m_height, 0.1f, 256.0f);
    mvp.viewMatrix = glm::mat4(glm::mat3(m_camera.m_viewMat));
    mvp.modelMatrix = glm::mat4(1.0f);
    Tools::mapMemory(m_skyboxUniformMemory, sizeof(Uniform), &mvp);
    
    // blur params
    uniformSize = sizeof(BlurParams);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_blurParamUniformBuffer, m_blurParamUniformMemory);
    BlurParams params = {};
    params.blurScale = 2.0f;
    params.blurStrength = 3.0f;
    Tools::mapMemory(m_blurParamUniformMemory, sizeof(BlurParams), &params);
}

void Bloom::prepareDescriptorSetLayoutAndPipelineLayout()
{
//    {
//        std::array<VkDescriptorSetLayoutBinding, 2> bindings;
//        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
//        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
//        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
//        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_blurDescriptorSetLayout));
//        createPipelineLayout(&m_blurDescriptorSetLayout, 1, m_blurPipelineLayout);
//    }
//
//    {
//        std::array<VkDescriptorSetLayoutBinding, 3> bindings;
//        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
//        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
//        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
//        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
//        createPipelineLayout();
//    }
    
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void Bloom::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 4;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 4);
    
    {
//        createDescriptorSet(&m_blurDescriptorSetLayout, 1, m_blurDescriptorSet[0]);
        createDescriptorSet(m_blurDescriptorSet[0]);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfo.buffer = m_blurParamUniformBuffer;
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offscreenColorImageView[0];
        imageInfo.sampler = m_offscreenColorSample;

        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_blurDescriptorSet[0], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_blurDescriptorSet[0], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
//        createDescriptorSet(&m_blurDescriptorSetLayout, 1, m_blurDescriptorSet[1]);
        createDescriptorSet(m_blurDescriptorSet[1]);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfo.buffer = m_blurParamUniformBuffer;
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offscreenColorImageView[1];
        imageInfo.sampler = m_offscreenColorSample;

        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_blurDescriptorSet[1], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_blurDescriptorSet[1], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_objectDescriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfo.buffer = m_objectUniformBuffer;
        
        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
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
    }
}

void Bloom::createGraphicsPipeline()
{
    VkPipelineVertexInputStateCreateInfo emptyVertexInput = {};
    emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    emptyVertexInput.flags = 0;
    
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

    std::array<VkPipelineColorBlendAttachmentState, 1> colorBlendAttachments;
    colorBlendAttachments[0] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo( static_cast<uint32_t>(colorBlendAttachments.size()), colorBlendAttachments.data());

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

    createInfo.pVertexInputState = &emptyVertexInput;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    
    // bloom
    VkPipelineColorBlendAttachmentState state = {};
    state.blendEnable = VK_TRUE;
    state.colorWriteMask = 0xF;
    state.blendEnable = VK_TRUE;
    state.colorBlendOp = VK_BLEND_OP_ADD;
    state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    state.alphaBlendOp = VK_BLEND_OP_ADD;
    state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    
    VkPipelineColorBlendStateCreateInfo colorBlend1 = Tools::getPipelineColorBlendStateCreateInfo(1, &state);
    createInfo.pColorBlendState = &colorBlend1;
    
    int direction = 0;
    VkSpecializationMapEntry specializationMapEntry;
    specializationMapEntry.constantID = 0;
    specializationMapEntry.size = sizeof(uint32_t);
    specializationMapEntry.offset = 0;
    
    VkSpecializationInfo specializationInfo = {};
    specializationInfo.dataSize = sizeof(uint32_t);
    specializationInfo.mapEntryCount = 1;
    specializationInfo.pMapEntries = &specializationMapEntry;
    specializationInfo.pData = &direction;
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/gaussblur.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/gaussblur.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderStages[1].pSpecializationInfo = &specializationInfo;
    createInfo.renderPass = m_offscreenRenderPass;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_bloomPipeline[0]));
    
    direction = 1;
    createInfo.renderPass = m_renderPass;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_bloomPipeline[1]));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    
    // object
    createInfo.pVertexInputState = m_skyboxLoader.getPipelineVertexInputState();
    state.blendEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_TRUE;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/phongpass.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/phongpass.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_objectPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/colorpass.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/colorpass.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    createInfo.renderPass = m_offscreenRenderPass;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_glowPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // skybox
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/skybox.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "bloom/skybox.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    depthStencil.depthWriteEnable = VK_FALSE;
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    createInfo.renderPass = m_renderPass;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_skyboxPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void Bloom::updateRenderData()
{
    static float timer = 0.0f;
    timer += 0.001f;
    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat = glm::translate(modelMat, glm::vec3(sin(glm::radians(timer * 360.0f)) * 0.25f, -1.0f, cos(glm::radians(timer * 360.0f)) * 0.25f));
    modelMat = glm::rotate(modelMat, -sinf(glm::radians(timer * 360.0f)) * 0.15f, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMat = glm::rotate(modelMat, glm::radians(timer * 360.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = modelMat;
    Tools::mapMemory(m_objectUniformMemory, sizeof(Uniform), &mvp);
}

void Bloom::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    m_skyboxLoader.bindBuffers(commandBuffer);
    m_skyboxLoader.draw(commandBuffer);
    
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_objectDescriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_objectPipeline);
    m_objectLoader.bindBuffers(commandBuffer);
    m_objectLoader.draw(commandBuffer);
    
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_blurDescriptorSet[1], 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomPipeline[1]);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

}

void Bloom::createOtherBuffer()
{
    for(uint32_t i = 0; i < 2; ++i)
    {
        Tools::createImageAndMemoryThenBind(m_offscreenColorFormat, m_offscrrenWidth, m_offscrrenHeight, 1, 1,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_offscreenColorImage[i], m_offscreenColorMemory[i]);
        Tools::createImageView(m_offscreenColorImage[i], m_offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_offscreenColorImageView[i]);
    }
    
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_offscreenColorSample);
    
    for(uint32_t i = 0; i < 2; ++i)
    {
        Tools::createImageAndMemoryThenBind(m_offscreenDepthFormat, m_offscrrenWidth, m_offscrrenHeight, 1, 1,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_offscreenDepthImage[i], m_offscreenDepthMemory[i]);
        
        Tools::createImageView(m_offscreenDepthImage[i], m_offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_offscreenDepthImageView[i]);
    }
}

void Bloom::createOffscreenRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachmentDescription;
    attachmentDescription[0] = Tools::getAttachmentDescription(m_offscreenColorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    attachmentDescription[1] = Tools::getAttachmentDescription(m_offscreenDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
 
    VkAttachmentReference colorAttachmentReference[1] = {};
    colorAttachmentReference[0].attachment = 0;
    colorAttachmentReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = colorAttachmentReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.attachmentCount = static_cast<uint32_t>(attachmentDescription.size());
    createInfo.pAttachments = attachmentDescription.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    
    VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_offscreenRenderPass) );
}

void Bloom::createOffscreenFrameBuffer()
{
    std::array<VkImageView, 2> attachments;
    attachments[0] = m_offscreenColorImageView[0];
    attachments[1] = m_offscreenDepthImageView[0];
    
    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = m_offscreenRenderPass;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.width = m_offscrrenWidth;
    createInfo.height = m_offscrrenHeight;
    createInfo.layers = 1;
    VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_offscreenFrameBuffer[0]));
    
    attachments[0] = m_offscreenColorImageView[1];
    attachments[1] = m_offscreenDepthImageView[1];
    createInfo.pAttachments = attachments.data();
    VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_offscreenFrameBuffer[1]));
}

void Bloom::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_offscreenRenderPass;
    passBeginInfo.framebuffer = m_offscreenFrameBuffer[0];
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = m_offscrrenWidth;
    passBeginInfo.renderArea.extent.height = m_offscrrenHeight;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_offscrrenWidth, m_offscrrenHeight);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = m_offscrrenWidth;
    scissor.extent.height = m_offscrrenHeight;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_objectDescriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_glowPipeline);
    m_glowLoader.bindBuffers(commandBuffer);
    m_glowLoader.draw(commandBuffer);
    
    vkCmdEndRenderPass(commandBuffer);
    
    
    
    passBeginInfo.framebuffer = m_offscreenFrameBuffer[1];
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_blurDescriptorSet[0], 0, NULL);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomPipeline[0]);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    
    vkCmdEndRenderPass(commandBuffer);
}


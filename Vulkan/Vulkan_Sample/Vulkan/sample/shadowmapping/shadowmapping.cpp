
#include "shadowmapping.h"

ShadowMapping::ShadowMapping(std::string title) : Application(title)
{
}

ShadowMapping::~ShadowMapping()
{}

void ShadowMapping::init()
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

void ShadowMapping::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, -0.0f, -20.0f));
    m_camera.setRotation(glm::vec3(-15.0f, -390.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void ShadowMapping::setEnabledFeatures()
{
//    if(m_deviceFeatures.samplerAnisotropy)
//    {
//        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
//    }
}

void ShadowMapping::clear()
{
    vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_shadowDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
    
    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_offscreenFrameBuffer, nullptr);
    vkDestroyImageView(m_device, m_offscreenDepthImageView, nullptr);
    vkDestroyImage(m_device, m_offscreenDepthImage, nullptr);
    vkFreeMemory(m_device, m_offscreenDepthMemory, nullptr);
    vkDestroySampler(m_device, m_offscreenDepthSampler, nullptr);

    vkFreeMemory(m_device, m_shadowUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_shadowUniformBuffer, nullptr);

    vkDestroyPipeline(m_device, m_debugPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    
    m_sceneLoader.clear();
    Application::clear();
}

void ShadowMapping::prepareVertex()
{
    std::vector<std::string> filenames = { "samplescene.gltf", "vulkanscene_shadow.gltf"};
    m_sceneLoader.loadFromFile(Tools::getModelPath() + filenames[0], m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY);
    m_sceneLoader.createVertexAndIndexBuffer();
    m_sceneLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal});
}

void ShadowMapping::prepareUniform()
{
    // shadow map
    VkDeviceSize uniformSize = sizeof(ShadowUniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_shadowUniformBuffer, m_shadowUniformMemory);

    updateLightPos();
    updateShadowMapMVP();
    
    // debug or scene
    uniformSize = sizeof(Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::mat4(1.0f);
    mvp.shadowMapMvp = m_shadowUniformMvp.shadowMVP;
    mvp.zNear = m_zNear;
    mvp.zFar = m_zFar;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void ShadowMapping::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        VkDescriptorSetLayoutBinding binding;
        binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_shadowDescriptorSetLayout));
        createPipelineLayout(&m_shadowDescriptorSetLayout, 1, m_shadowPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings.data(), 2);
        createPipelineLayout();
    }
}

void ShadowMapping::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    
    {
        createDescriptorSet(&m_shadowDescriptorSetLayout, 1, m_shadowDescriptorSet);

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfo.buffer = m_shadowUniformBuffer;

        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_shadowDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offscreenDepthImageView;
        imageInfo.sampler = m_offscreenDepthSampler;
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
//    {
//        createDescriptorSet(m_compositionDescriptorSet);
//
//        VkDescriptorImageInfo imageInfo1 = {};
//        imageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//        imageInfo1.imageView = m_offscreenColorImageView[0];
//        imageInfo1.sampler = m_offscreenColorSample;
//
//        VkDescriptorImageInfo imageInfo2 = {};
//        imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//        imageInfo2.imageView = m_offscreenColorImageView[1];
//        imageInfo2.sampler = m_offscreenColorSample;
//
//        std::array<VkWriteDescriptorSet, 2> writes = {};
//        writes[0] = Tools::getWriteDescriptorSet(m_compositionDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo1);
//        writes[1] = Tools::getWriteDescriptorSet(m_compositionDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo2);
//        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
//    }
}

void ShadowMapping::createGraphicsPipeline()
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

    std::array<VkPipelineColorBlendAttachmentState, 1> colorBlendAttachments;
    colorBlendAttachments[0] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo( static_cast<uint32_t>(colorBlendAttachments.size()), colorBlendAttachments.data());

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
//    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

//    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

    createInfo.pVertexInputState = m_sceneLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    createInfo.layout = m_shadowPipelineLayout;
    createInfo.renderPass = m_offscreenRenderPass;
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmapping/offscreen.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmapping/offscreen.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_shadowPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //debug.
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmapping/quad.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmapping/quad.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_debugPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
//    createInfo.pVertexInputState = m_sceneLoader.getPipelineVertexInputState();
//    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
//    skyboxOrObjectIndex = 1;
//    depthStencil.depthWriteEnable = VK_TRUE;
//    depthStencil.depthTestEnable = VK_TRUE;
//    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_objectPipeline));
//    vkDestroyShaderModule(m_device, vertModule, nullptr);
//    vkDestroyShaderModule(m_device, fragModule, nullptr);

    // composition
//    VkPipelineVertexInputStateCreateInfo emptyVertexInput = {};
//    emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//    emptyVertexInput.flags = 0;
//    rasterization.cullMode = VK_CULL_MODE_NONE;
//    createInfo.pVertexInputState = &emptyVertexInput;
//    createInfo.layout = m_pipelineLayout;
//    createInfo.renderPass = m_renderPass;
//    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
//    VkPipelineColorBlendStateCreateInfo colorBlend2 = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
//    createInfo.pColorBlendState = &colorBlend2;
//    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/composition.vert.spv");
//    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/composition.frag.spv");
//    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
//    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
//    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_compositionPipeline));
//    vkDestroyShaderModule(m_device, vertModule, nullptr);
//    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // bloom
//    VkPipelineColorBlendAttachmentState state = {};
//    state.blendEnable = VK_TRUE;
//    state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
//    state.colorBlendOp = VK_BLEND_OP_ADD;
//    state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
//    state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
//    state.alphaBlendOp = VK_BLEND_OP_ADD;
//    state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
//    state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
//
//    VkPipelineColorBlendStateCreateInfo colorBlend3 = Tools::getPipelineColorBlendStateCreateInfo(1, &state);
//    createInfo.pColorBlendState = &colorBlend3;
//
//    int direction = 0;
//    specializationInfo.pData = &direction;
//    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/bloom.vert.spv");
//    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/bloom.frag.spv");
//    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
//    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
//    shaderStages[1].pSpecializationInfo = &specializationInfo;
//    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_bloomPipeline[0]));
//
//    direction = 1;
//    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_bloomPipeline[1]));
//    vkDestroyShaderModule(m_device, vertModule, nullptr);
//    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void ShadowMapping::updateRenderData()
{
}

void ShadowMapping::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

// ================ shadow mapping ======================

void ShadowMapping::updateLightPos()
{
    float passTime = 0.1;
    m_lightPos.x = cos(glm::radians(passTime * 360.0f)) * 40.0f;
    m_lightPos.y = -50.0f + sin(glm::radians(passTime * 360.0f)) * 20.0f;
    m_lightPos.z = 25.0f + sin(glm::radians(passTime * 360.0f)) * 5.0f;
}

void ShadowMapping::updateShadowMapMVP()
{
    // Matrix from light's point of view
    glm::mat4 depthProjectionMatrix = glm::perspective(glm::radians(m_lightFOV), 1.0f, m_zNear, m_zFar);
    glm::mat4 depthViewMatrix = glm::lookAt(m_lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
    glm::mat4 depthModelMatrix = glm::mat4(1.0f);

    m_shadowUniformMvp.shadowMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
    Tools::mapMemory(m_shadowUniformMemory, sizeof(ShadowUniform), &m_shadowUniformMvp);
}

void ShadowMapping::createOtherBuffer()
{
    // depth
    Tools::createImageAndMemoryThenBind(m_offscreenDepthFormat, m_shadowMapWidth, m_shadowMapHeight, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_offscreenDepthImage, m_offscreenDepthMemory);
    
    Tools::createImageView(m_offscreenDepthImage, m_offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_offscreenDepthImageView);
    
    // sample
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_offscreenDepthSampler);
}

void ShadowMapping::createOffscreenRenderPass()
{
    VkAttachmentDescription attachmentDescription;
    attachmentDescription = Tools::getAttachmentDescription(m_offscreenDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
 
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 0;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 0;
    subpassDescription.pColorAttachments = nullptr;
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
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &attachmentDescription;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    
    VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_offscreenRenderPass) );
}

void ShadowMapping::createOffscreenFrameBuffer()
{
    std::vector<VkImageView> attachments;
    attachments.push_back(m_offscreenDepthImageView);
    
    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = m_offscreenRenderPass;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.width = m_shadowMapWidth;
    createInfo.height = m_shadowMapHeight;
    createInfo.layers = 1;
    
    VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_offscreenFrameBuffer));
}

void ShadowMapping::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    std::array<VkClearValue, 1> clearValues = {};
    clearValues[0].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_offscreenRenderPass;
    passBeginInfo.framebuffer = m_offscreenFrameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = m_shadowMapWidth;
    passBeginInfo.renderArea.extent.height = m_shadowMapHeight;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_shadowMapWidth, m_shadowMapHeight);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = m_shadowMapWidth;
    scissor.extent.height = m_shadowMapHeight;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelineLayout, 0, 1, &m_shadowDescriptorSet, 0, nullptr);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
    m_sceneLoader.bindBuffers(commandBuffer);
    m_sceneLoader.draw(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);
}

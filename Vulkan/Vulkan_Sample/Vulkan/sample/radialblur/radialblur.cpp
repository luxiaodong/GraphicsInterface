
#include "radialblur.h"

RadialBlur::RadialBlur(std::string title) : Application(title)
{
}

RadialBlur::~RadialBlur()
{}

void RadialBlur::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createOffScreenRenderPass();
    createOffScreenFrameBuffer();
    
    createGraphicsPipeline();
}

void RadialBlur::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -17.5f));
    m_camera.setRotation(glm::vec3(-16.25f, -28.75f, 0.0f));
    m_camera.setRotationSpeed(0.5f);
    m_camera.setPerspective(45.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void RadialBlur::setEnabledFeatures()
{
//    if(m_deviceFeatures.shaderClipDistance)
//    {
//        m_deviceEnabledFeatures.shaderClipDistance = VK_TRUE;
//    }
}

void RadialBlur::clear()
{
    vkDestroyPipeline(m_device, m_offScreenPipeline, nullptr);
    vkDestroyRenderPass(m_device, m_offScreenRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_offScreenFrameBuffer, nullptr);
    vkFreeMemory(m_device, m_offScreenUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_offScreenUniformBuffer, nullptr);
    
    vkDestroyImageView(m_device, m_offScreenColorImageView, nullptr);
    vkDestroyImage(m_device, m_offScreenColorImage, nullptr);
    vkFreeMemory(m_device, m_offScreenColorMemory, nullptr);
    vkDestroySampler(m_device, m_offScreenColorSampler, nullptr);
    vkDestroyImageView(m_device, m_offScreenDepthImageView, nullptr);
    vkDestroyImage(m_device, m_offScreenDepthImage, nullptr);
    vkFreeMemory(m_device, m_offScreenDepthMemory, nullptr);
    
    vkDestroyPipeline(m_device, m_phongPipeline, nullptr);
    vkDestroyPipeline(m_device, m_colorPipeline, nullptr);
    vkDestroyPipeline(m_device, m_radialBlurPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_radialBlurPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_radialBlurDescriptorSetLayout, nullptr);
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_blurParamsMemory, nullptr);
    vkDestroyBuffer(m_device, m_blurParamsBuffer, nullptr);

    m_objectLoader.clear();
    m_pGradient->clear();
    delete m_pGradient;
    Application::clear();
}

void RadialBlur::prepareVertex()
{
    uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    m_objectLoader.loadFromFile(Tools::getModelPath() + "glowsphere.gltf", m_graphicsQueue, flags);
    m_objectLoader.createVertexAndIndexBuffer();
    m_objectLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal});
    
    m_pGradient = Texture::loadTextrue2D(Tools::getTexturePath() + "particle_gradient_rgba.ktx", m_graphicsQueue);
}

void RadialBlur::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.gradientPos = 0.0f;
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
    
    // blur params
    uniformSize = sizeof(BlurParams);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_blurParamsBuffer, m_blurParamsMemory);
    BlurParams params;
    params.radialBlurScale = 0.35f;
    params.radialBlurStrength = 0.75f;
    params.radialOrigin = glm::vec2(0.5f, 0.5f);
    Tools::mapMemory(m_blurParamsMemory, uniformSize, &params);
}

void RadialBlur::createOtherBuffer()
{
    {
        Tools::createImageAndMemoryThenBind(m_surfaceFormatKHR.format, m_offScreenWidth, m_offScreenHeight, 1, 1,
                                     VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                     VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_offScreenColorImage, m_offScreenColorMemory);
        Tools::createImageView(m_offScreenColorImage, m_surfaceFormatKHR.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_offScreenColorImageView);
        Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_offScreenColorSampler);
        
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tools::setImageLayout(cmd, m_offScreenColorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    }
    
    {
        Tools::createImageAndMemoryThenBind(m_depthFormat, m_offScreenWidth, m_offScreenHeight, 1, 1,
                                     VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                     VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_offScreenDepthImage, m_offScreenDepthMemory);
        Tools::createImageView(m_offScreenDepthImage, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_offScreenDepthImageView);
        
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tools::setImageLayout(cmd, m_offScreenDepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    }
}

void RadialBlur::prepareDescriptorSetLayoutAndPipelineLayout()
{
    // radialBlur
    {
        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings, 2);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_radialBlurDescriptorSetLayout));
        createPipelineLayout(&m_radialBlurDescriptorSetLayout, 1, m_radialBlurPipelineLayout);
    }
    
    // scene
    {
        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings, 2);
        createPipelineLayout();
    }
}

void RadialBlur::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    
    // radialBlur
    {
        createDescriptorSet(&m_radialBlurDescriptorSetLayout, 1, m_radialBlurDescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(BlurParams);
        bufferInfo.buffer = m_blurParamsBuffer;
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offScreenColorImageView;
        imageInfo.sampler = m_offScreenColorSampler;
        
        VkWriteDescriptorSet writes[2] = {};
        writes[0] = Tools::getWriteDescriptorSet(m_radialBlurDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_radialBlurDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
    }
    
    // scene
    {
        createDescriptorSet(m_objectDescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pGradient->getDescriptorImageInfo();
        
        VkWriteDescriptorSet writes[2] = {};
        writes[0] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
    }
}

void RadialBlur::createGraphicsPipeline()
{
    VkPipelineVertexInputStateCreateInfo emptyInputState = {};
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_TRUE, 0xf);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

    createInfo.pVertexInputState = &emptyInputState;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "radialblur/radialblur.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "radialblur/radialblur.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    createInfo.layout = m_radialBlurPipelineLayout;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_radialBlurPipeline));
    colorBlendAttachment.blendEnable = VK_FALSE;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_offScreenPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "radialblur/phongpass.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "radialblur/phongpass.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    createInfo.pVertexInputState = m_objectLoader.getPipelineVertexInputState();
    createInfo.layout = m_pipelineLayout;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_phongPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);

    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "radialblur/colorpass.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "radialblur/colorpass.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    createInfo.renderPass = m_offScreenRenderPass;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_colorPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void RadialBlur::updateRenderData()
{
    static float dt = 0.001;
    dt += 0.001;
    
    m_camera.setRotation(m_camera.m_rotation + glm::vec3(0.0f, 1.0f, 0.0f));
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.gradientPos = dt;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void RadialBlur::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_phongPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_objectDescriptorSet, 0, NULL);
    m_objectLoader.bindBuffers(commandBuffer);
    m_objectLoader.draw(commandBuffer);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_radialBlurPipelineLayout, 0, 1, &m_radialBlurDescriptorSet, 0, NULL);
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_offScreenPipeline);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_radialBlurPipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void RadialBlur::createOffScreenRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachmentDescription;
    attachmentDescription[0] = Tools::getAttachmentDescription(m_surfaceFormatKHR.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    attachmentDescription[1] = Tools::getAttachmentDescription(m_depthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
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
    
    if( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_offScreenRenderPass) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create renderpass!");
    }
}

void RadialBlur::createOffScreenFrameBuffer()
{
    std::vector<VkImageView> attachments;
    attachments.push_back(m_offScreenColorImageView);
    attachments.push_back(m_offScreenDepthImageView);
    
    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = m_offScreenRenderPass;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.width = m_offScreenWidth;
    createInfo.height = m_offScreenHeight;
    createInfo.layers = 1;
    
    if( vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_offScreenFrameBuffer) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create frame buffer!");
    }
}

void RadialBlur::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_offScreenRenderPass;
    passBeginInfo.framebuffer = m_offScreenFrameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = m_offScreenWidth;
    passBeginInfo.renderArea.extent.height = m_offScreenHeight;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_offScreenWidth, m_offScreenHeight);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = m_offScreenWidth;
    scissor.extent.height = m_offScreenHeight;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_colorPipeline);
    m_objectLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_objectDescriptorSet, 0, nullptr);
    m_objectLoader.draw(commandBuffer);
    
    vkCmdEndRenderPass(commandBuffer);
}

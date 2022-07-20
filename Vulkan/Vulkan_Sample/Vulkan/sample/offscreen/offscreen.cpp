
#include "offscreen.h"

OffScreen::OffScreen(std::string title) : Application(title)
{
}

OffScreen::~OffScreen()
{}

void OffScreen::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createMirrorRenderPass();
    createMirrorFrameBuffer();
    
    createGraphicsPipeline();
}

void OffScreen::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 1.0f, -6.0f));
    m_camera.setRotation(glm::vec3(-2.5f, 0.0f, 0.0f));
    m_camera.setRotationSpeed(0.5f);
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void OffScreen::setEnabledFeatures()
{
    if(m_deviceFeatures.shaderClipDistance)
    {
        m_deviceEnabledFeatures.shaderClipDistance = VK_TRUE;
    }
}

void OffScreen::clear()
{
    // pass 1
    vkDestroyPipeline(m_device, m_mirrorPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_mirrorPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_mirrorDescriptorSetLayout, nullptr);
    vkDestroyRenderPass(m_device, m_mirrorRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_mirrorFrameBuffer, nullptr);
    vkFreeMemory(m_device, m_mirrorUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_mirrorUniformBuffer, nullptr);
    
    vkDestroyImageView(m_device, m_mirrorColorImageView, nullptr);
    vkDestroyImage(m_device, m_mirrorColorImage, nullptr);
    vkFreeMemory(m_device, m_mirrorColorMemory, nullptr);
    vkDestroySampler(m_device, m_mirrorColorSampler, nullptr);
    vkDestroyImageView(m_device, m_mirrorDepthImageView, nullptr);
    vkDestroyImage(m_device, m_mirrorDepthImage, nullptr);
    vkFreeMemory(m_device, m_mirrorDepthMemory, nullptr);
    
    // pass 2
    vkDestroyPipeline(m_device, m_debugPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_debugPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_debugDescriptorSetLayout, nullptr);

    // pass 4
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);

    m_dragonLoader.clear();
    Application::clear();
}

void OffScreen::prepareVertex()
{
    m_dragonLoader.loadFromFile(Tools::getModelPath() + "chinesedragon.gltf", m_graphicsQueue);
    m_dragonLoader.createVertexAndIndexBuffer();
    m_dragonLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Color, VertexComponent::Normal});
    
//    m_planeLoader.loadFromFile(Tools::getModelPath() + "plane.gltf", m_graphicsQueue);
}

void OffScreen::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    // pass 4
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    Uniform mvp = {};
    mvp.modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    mvp.modelMatrix = glm::translate(mvp.modelMatrix, glm::vec3(0.0f, -1.0f, 0.0f));
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
    
    // pass 1
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_mirrorUniformBuffer, m_mirrorUniformMemory);
    
    mvp.modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    mvp.modelMatrix = glm::scale(mvp.modelMatrix, glm::vec3(1.0f, -1.0f, 1.0f));
    mvp.modelMatrix = glm::translate(mvp.modelMatrix, glm::vec3(0.0f, -1.0f, 0.0f));
    Tools::mapMemory(m_mirrorUniformMemory, uniformSize, &mvp);
}

void OffScreen::createOtherBuffer()
{
    {
        Tools::createImageAndMemoryThenBind(m_surfaceFormatKHR.format, m_mirrorWidth, m_mirrorHeight, 1, 1,
                                     VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                     VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_mirrorColorImage, m_mirrorColorMemory);
        Tools::createImageView(m_mirrorColorImage, m_surfaceFormatKHR.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_mirrorColorImageView);
        Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_mirrorColorSampler);
        
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tools::setImageLayout(cmd, m_mirrorColorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    }
    
    {
        Tools::createImageAndMemoryThenBind(VK_FORMAT_D32_SFLOAT, m_mirrorWidth, m_mirrorHeight, 1, 1,
                                     VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                     VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_mirrorDepthImage, m_mirrorDepthMemory);
        Tools::createImageView(m_mirrorDepthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_mirrorDepthImageView);
        
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tools::setImageLayout(cmd, m_mirrorDepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    }
}

void OffScreen::prepareDescriptorSetLayoutAndPipelineLayout()
{
    // pass 1 mirror
    {
        VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_mirrorDescriptorSetLayout));
        createPipelineLayout(&m_mirrorDescriptorSetLayout, 1, m_mirrorPipelineLayout);
    }
    
    // pass 2 debug
    {
        VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_debugDescriptorSetLayout));
        createPipelineLayout(&m_debugDescriptorSetLayout, 1, m_debugPipelineLayout);
    }
    
    // pass 4
    {
        VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        createDescriptorSetLayout(&binding, 1);
        createPipelineLayout();
    }
}

void OffScreen::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
//    poolSizes[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
//    poolSizes[1].descriptorCount = 2;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 3);
    
    //pass 1
    {
        createDescriptorSet(&m_mirrorDescriptorSetLayout, 1, m_mirrorDescriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_mirrorUniformBuffer;
        
        VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_mirrorDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
    
    //pass 2 debug
    {
        createDescriptorSet(&m_debugDescriptorSetLayout, 1, m_debugDescriptorSet);
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_mirrorColorImageView;
        imageInfo.sampler = m_mirrorColorSampler;
        
        VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_debugDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
    
    //pass 4
    {
        createDescriptorSet(m_descriptorSet);
            
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
}

void OffScreen::createGraphicsPipeline()
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
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
    
    // pass 2
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "offscreen/quad.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "offscreen/quad.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    rasterization.cullMode = VK_CULL_MODE_NONE;
    createInfo.layout = m_debugPipelineLayout;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_debugPipeline));
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);

    // pass 4
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "offscreen/phong.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "offscreen/phong.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    createInfo.layout = m_pipelineLayout;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    
    // pass 1
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    createInfo.renderPass = m_mirrorRenderPass;
    createInfo.layout = m_mirrorPipelineLayout;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_mirrorPipeline));

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
//    // subpass 1
//    VkPipelineVertexInputStateCreateInfo emptyInputState = {};
//    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//    createInfo.subpass = 1;
//    createInfo.pVertexInputState = &emptyInputState;
//    createInfo.layout = m_readPipelineLayout;
//    rasterization.cullMode = VK_CULL_MODE_NONE;
//    depthStencil.depthWriteEnable = VK_FALSE;
//
//    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "inputattachments/attachmentread.vert.spv");
//    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "inputattachments/attachmentread.frag.spv");
//    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
//    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
//
//    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_readPipeline) != VK_SUCCESS )
//    {
//        throw std::runtime_error("failed to create graphics pipeline!");
//    }
//
//    vkDestroyShaderModule(m_device, vertModule, nullptr);
//    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void OffScreen::updateRenderData()
{
//    Uniform mvp = {};
//    mvp.modelMatrix = glm::mat4(1.0f);
//    mvp.viewMatrix = m_camera.m_viewMat;
//    mvp.projectionMatrix = m_camera.m_projMat;
//    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void OffScreen::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    // pass 1
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mirrorPipeline);
//    m_dragonLoader.bindBuffers(commandBuffer);
//    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mirrorPipelineLayout, 0, 1, &m_mirrorDescriptorSet, 0, nullptr);
//    m_dragonLoader.draw(commandBuffer);
    
    // pass 2
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipelineLayout, 0, 1, &m_debugDescriptorSet, 0, NULL);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    
    // pass 4
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
//    m_dragonLoader.bindBuffers(commandBuffer);
//    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
//    m_dragonLoader.draw(commandBuffer);
}

void OffScreen::createMirrorRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachmentDescription;
    attachmentDescription[0] = Tools::getAttachmentDescription(m_surfaceFormatKHR.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    attachmentDescription[1] = Tools::getAttachmentDescription(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
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
    
    if( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_mirrorRenderPass) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create renderpass!");
    }
}

void OffScreen::createMirrorFrameBuffer()
{
    std::vector<VkImageView> attachments;
    attachments.push_back(m_mirrorColorImageView);
    attachments.push_back(m_mirrorDepthImageView);
    
    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = m_mirrorRenderPass;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.width = m_mirrorWidth;
    createInfo.height = m_mirrorHeight;
    createInfo.layers = 1;
    
    if( vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_mirrorFrameBuffer) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create frame buffer!");
    }
}

void OffScreen::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_mirrorRenderPass;
    passBeginInfo.framebuffer = m_mirrorFrameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = m_mirrorWidth;
    passBeginInfo.renderArea.extent.height = m_mirrorHeight;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_mirrorWidth, m_mirrorHeight);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = m_mirrorWidth;
    scissor.extent.height = m_mirrorHeight;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mirrorPipeline);
    m_dragonLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mirrorPipelineLayout, 0, 1, &m_mirrorDescriptorSet, 0, nullptr);
    m_dragonLoader.draw(commandBuffer);
    
    vkCmdEndRenderPass(commandBuffer);
}


#include "pointlightshadow.h"

PointLightShadow::PointLightShadow(std::string title) : Application(title)
{
}

PointLightShadow::~PointLightShadow()
{}

void PointLightShadow::init()
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

void PointLightShadow::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.5f, -15.0f));
    m_camera.setRotation(glm::vec3(-20.5f, -673.0f, 0.0f));
    m_camera.setPerspective(45.0f, (float)m_width / (float)m_height, m_zNear, m_zFar);
}

void PointLightShadow::setEnabledFeatures()
{
//    if(m_deviceFeatures.samplerAnisotropy)
//    {
//        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
//    }
}

void PointLightShadow::clear()
{
    vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_shadowDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
    
    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_offscreenFrameBuffer, nullptr);
    vkDestroyImageView(m_device, m_offscreenDepthImageView, nullptr);
    vkDestroyImage(m_device, m_offscreenDepthImage, nullptr);
    vkFreeMemory(m_device, m_offscreenDepthMemory, nullptr);
    vkDestroyImageView(m_device, m_offscreenColorImageView, nullptr);
    vkDestroyImage(m_device, m_offscreenColorImage, nullptr);
    vkFreeMemory(m_device, m_offscreenColorMemory, nullptr);

    vkDestroyImage(m_device, m_cubeImage, nullptr);
    vkDestroyImageView(m_device, m_cubeImageView, nullptr);
    vkFreeMemory(m_device, m_cubeMemory, nullptr);
    vkDestroySampler(m_device, m_cubeSampler, nullptr);
    
    vkFreeMemory(m_device, m_shadowUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_shadowUniformBuffer, nullptr);

    vkDestroyPipeline(m_device, m_debugPipeline, nullptr);
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    
    m_sceneLoader.clear();
    m_cubeLoader.clear();
    Application::clear();
}

void PointLightShadow::prepareVertex()
{
    m_sceneLoader.loadFromFile(Tools::getModelPath() + "shadowscene_fire.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY );
    m_sceneLoader.createVertexAndIndexBuffer();
    m_sceneLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Color, VertexComponent::Normal});

    m_cubeLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY );
    m_cubeLoader.createVertexAndIndexBuffer();
    m_cubeLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Color, VertexComponent::Normal});
}

void PointLightShadow::prepareUniform()
{
    // shadow map
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_shadowUniformBuffer, m_shadowUniformMemory);
    updateShadowMapMVP();
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    updateUniformMVP();
}

void PointLightShadow::prepareDescriptorSetLayoutAndPipelineLayout()
{
    m_shadowPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    m_shadowPushConstantRange.offset = 0;
    m_shadowPushConstantRange.size = sizeof(PushConstantData);
    
    {
        VkDescriptorSetLayoutBinding binding;
        binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_shadowDescriptorSetLayout));
        createPipelineLayout(&m_shadowDescriptorSetLayout, 1, m_shadowPipelineLayout, &m_shadowPushConstantRange, 1);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings.data(), 2);
        createPipelineLayout();
    }
}

void PointLightShadow::prepareDescriptorSetAndWrite()
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
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_cubeImageView;
        imageInfo.sampler = m_cubeSampler;

        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void PointLightShadow::createGraphicsPipeline()
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
        VK_DYNAMIC_STATE_SCISSOR,
//        VK_DYNAMIC_STATE_DEPTH_BIAS
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
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingomni/offscreen.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingomni/offscreen.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_shadowPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //debug.
    //dynamicStates.pop_back();
//    rasterization.depthBiasEnable = VK_FALSE;
    VkPipelineVertexInputStateCreateInfo emptyInputState = {};
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.pVertexInputState = &emptyInputState;
//    createInfo.pVertexInputState = m_cubeLoader.getPipelineVertexInputState();
    rasterization.cullMode = VK_CULL_MODE_NONE;
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingomni/cubemapdisplay.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingomni/cubemapdisplay.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_debugPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //scene.
    createInfo.pVertexInputState = m_cubeLoader.getPipelineVertexInputState();
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingomni/scene.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingomni/scene.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void PointLightShadow::updateRenderData()
{
    updateShadowMapMVP();
    updateUniformMVP();
}

void PointLightShadow::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    
    if(m_isShowDebug)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    else
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
        m_sceneLoader.bindBuffers(commandBuffer);
        m_sceneLoader.draw(commandBuffer);
    }
}

// ================= shadow mapping ======================

void PointLightShadow::updateShadowMapMVP()
{
    static float passTime = 0.001;
    m_lightPos.x = sin(glm::radians(passTime * 360.0f)) * 0.15f;
    m_lightPos.z = cos(glm::radians(passTime * 360.0f)) * 0.15f;
//    passTime += 0.001f;

    Uniform mvp = {};
    mvp.projectionMatrix = glm::perspective((float)(M_PI/2.0), 1.0f, m_zNear, m_zFar);
    mvp.viewMatrix = glm::mat4(1.0f);
    mvp.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-m_lightPos.x, -m_lightPos.y, -m_lightPos.z));
    mvp.lightPos = m_lightPos;
    
    Tools::mapMemory(m_shadowUniformMemory, sizeof(Uniform), &mvp);
}

void PointLightShadow::updateUniformMVP()
{
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::mat4(1.0f);
    mvp.lightPos = m_lightPos;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void PointLightShadow::createOtherBuffer()
{
    {
        Tools::createImageAndMemoryThenBind(m_offscreenColorFormat, m_shadowMapWidth, m_shadowMapHeight, 1, 1,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_offscreenColorImage, m_offscreenColorMemory);
        Tools::createImageView(m_offscreenColorImage, m_offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_offscreenColorImageView);
    }
    
    {
        Tools::createImageAndMemoryThenBind(m_offscreenDepthFormat, m_shadowMapWidth, m_shadowMapHeight, 1, 1,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_offscreenDepthImage, m_offscreenDepthMemory);
        
        Tools::createImageView(m_offscreenDepthImage, m_offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_offscreenDepthImageView);
    }
    
    {
        Tools::createImageAndMemoryThenBind(m_offscreenColorFormat, m_shadowMapWidth, m_shadowMapHeight, 1, 6,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            m_cubeImage, m_cubeMemory, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

        Tools::createImageView(m_cubeImage, m_offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 6, m_cubeImageView, VK_IMAGE_VIEW_TYPE_CUBE);
        Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_cubeSampler);
        
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        VkImageSubresourceRange cubeSubresourceRange = {};
        cubeSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cubeSubresourceRange.baseArrayLayer = 0;
        cubeSubresourceRange.layerCount = 6;
        cubeSubresourceRange.baseMipLevel = 0;
        cubeSubresourceRange.levelCount = 1;
        
        Tools::setImageLayout(cmd, m_cubeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, cubeSubresourceRange);
        
        Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    }
}

void PointLightShadow::createOffscreenRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachmentDescription;
    attachmentDescription[0] = Tools::getAttachmentDescription(m_offscreenColorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    attachmentDescription[1] = Tools::getAttachmentDescription(m_offscreenDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
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
    
    VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_offscreenRenderPass) );
}

void PointLightShadow::createOffscreenFrameBuffer()
{
    std::vector<VkImageView> attachments;
    attachments.push_back(m_offscreenColorImageView);
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

void PointLightShadow::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    for(int i = 0; i < 6; ++i)
    {
        glm::mat4 viewMatrix = glm::mat4(1.0f);
        if(i == 0)
        {
            viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        else if(i == 1)
        {
            viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        else if(i == 2)
        {
            viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        else if(i == 3)
        {
            viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        else if(i == 4)
        {
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        else if(i == 5)
        {
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        }
        
        PushConstantData pushConstBlock;
        pushConstBlock.viewMatrix = viewMatrix;
        
        std::array<VkClearValue, 2> clearValues = {};
        clearValues[0].color = {0, 0, 0, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        
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
        vkCmdPushConstants(commandBuffer, m_shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &pushConstBlock);
        m_sceneLoader.bindBuffers(commandBuffer);
        m_sceneLoader.draw(commandBuffer);
        vkCmdEndRenderPass(commandBuffer);
        
        // setImageLayout && copy to cube
        Tools::setImageLayout(commandBuffer, m_offscreenColorImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        
        VkImageSubresourceRange cubeSubresourceRange = {};
        cubeSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cubeSubresourceRange.baseArrayLayer = i;
        cubeSubresourceRange.layerCount = 1;
        cubeSubresourceRange.baseMipLevel = 0;
        cubeSubresourceRange.levelCount = 1;
        
        Tools::setImageLayout(commandBuffer, m_cubeImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, cubeSubresourceRange);
        
        VkImageCopy copyRegion = {};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.baseArrayLayer = 0;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.srcSubresource.mipLevel = 0;
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.baseArrayLayer = i;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.dstSubresource.mipLevel = 0;
        copyRegion.srcOffset = {0, 0, 0};
        copyRegion.dstOffset = {0, 0, 0};
        copyRegion.extent.width = m_shadowMapWidth;
        copyRegion.extent.height = m_shadowMapHeight;
        copyRegion.extent.depth = 1;
        
        vkCmdCopyImage(commandBuffer, m_offscreenColorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_cubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
        
        Tools::setImageLayout(commandBuffer, m_offscreenColorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        
        Tools::setImageLayout(commandBuffer, m_cubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, cubeSubresourceRange);
    }
}


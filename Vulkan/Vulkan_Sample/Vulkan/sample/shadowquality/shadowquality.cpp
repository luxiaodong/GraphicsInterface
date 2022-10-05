
#include "shadowquality.h"

ShadowQuality::ShadowQuality(std::string title) : Application(title)
{
    m_width = 1336/2;
    m_height = 1018/2;
}

ShadowQuality::~ShadowQuality()
{}

void ShadowQuality::init()
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

void ShadowQuality::initCamera()
{
    m_camera.m_isFlipY = true;
    m_camera.lookAt(glm::vec3(-5.0f, 0.0f, 3.0f), glm::vec3(-4.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    m_camera.projection(73.6511383f, 1.31237721f, 0.1f, 1000.0f);
}

void ShadowQuality::setEnabledFeatures()
{
//    if(m_deviceFeatures.samplerAnisotropy)
//    {
//        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
//    }
}

void ShadowQuality::clear()
{
    vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_shadowDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
    
    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_offscreenFrameBuffer, nullptr);
    
    vkDestroyImageView(m_device, m_offscreenColorImageView, nullptr);
    vkDestroyImage(m_device, m_offscreenColorImage, nullptr);
    vkFreeMemory(m_device, m_offscreenColorMemory, nullptr);
    vkDestroySampler(m_device, m_offscreenColorSampler, nullptr);
    
    vkDestroyImageView(m_device, m_offscreenDepthImageView, nullptr);
    vkDestroyImage(m_device, m_offscreenDepthImage, nullptr);
    vkFreeMemory(m_device, m_offscreenDepthMemory, nullptr);
    vkDestroySampler(m_device, m_offscreenDepthSampler, nullptr);

    vkFreeMemory(m_device, m_shadowUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_shadowUniformBuffer, nullptr);

    vkDestroyPipeline(m_device, m_debugPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    
    m_sphereLoader.clear();
    m_planeLoader.clear();
    Application::clear();
}

void ShadowQuality::prepareVertex()
{
    std::vector<std::string> filenames = {"capsule.obj", "cone.obj", "convexmesh.obj", "cube.obj", "cylinder.obj", "sphere.obj"};
    
//    m_sphereLoader.loadFromFile(Tools::getModelPath() + "basic/" + filenames[5]);
    m_sphereLoader.loadCustomSphere();
    m_sphereLoader.createVertexAndIndexBuffer();
    
    m_planeLoader.loadFromFile(Tools::getModelPath() + "basic/floor.obj");
    m_planeLoader.createVertexAndIndexBuffer();
    
    Vertex::setVertexInputBindingDescription(0);
    Vertex::setVertexInputAttributeDescription(0,{VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal});
}

void ShadowQuality::prepareUniform()
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
    updateUniformMVP();
}

void ShadowQuality::prepareDescriptorSetLayoutAndPipelineLayout()
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

void ShadowQuality::prepareDescriptorSetAndWrite()
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
        
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offscreenColorImageView;
        imageInfo.sampler = m_offscreenColorSampler;

        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void ShadowQuality::createGraphicsPipeline()
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
    colorBlendAttachments[0] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, 0xf);
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
    
//    vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);
    rasterization.depthBiasConstantFactor = 1.25f;
    rasterization.depthBiasClamp = 0.0f;
    rasterization.depthBiasSlopeFactor = 1.75;
    rasterization.depthBiasEnable = VK_TRUE;

    createInfo.pVertexInputState = Vertex::getPipelineVertexInputState();
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
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowquality/offscreen.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowquality/offscreen.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_shadowPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //debug.
    //dynamicStates.pop_back();
    rasterization.depthBiasEnable = VK_FALSE;
    VkPipelineVertexInputStateCreateInfo emptyInputState = {};
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.pVertexInputState = &emptyInputState;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowquality/quad.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowquality/quad.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_debugPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //scene.
    int enablePCF = 0;
    VkSpecializationMapEntry specializationMapEntry;
    specializationMapEntry.constantID = 0;
    specializationMapEntry.size = sizeof(int);
    specializationMapEntry.offset = 0;
    
    VkSpecializationInfo specializationInfo = {};
    specializationInfo.dataSize = sizeof(int);
    specializationInfo.mapEntryCount = 1;
    specializationInfo.pMapEntries = &specializationMapEntry;
    specializationInfo.pData = &enablePCF;
    
    createInfo.pVertexInputState = Vertex::getPipelineVertexInputState();
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.depthBiasEnable = VK_FALSE;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowquality/scene.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowquality/scene.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderStages[1].pSpecializationInfo = &specializationInfo;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void ShadowQuality::updateRenderData()
{
    updateLightPos();
    updateShadowMapMVP();
    updateUniformMVP();
}

void ShadowQuality::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline);
//    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_sphereLoader.bindBuffers(commandBuffer);
    m_sphereLoader.draw(commandBuffer);
    
    m_planeLoader.bindBuffers(commandBuffer);
    m_planeLoader.draw(commandBuffer);
}

// ================= shadow mapping ======================

void ShadowQuality::updateLightPos()
{
    static float passTime = 0.5;
    m_lightPos.x = 25.0f + cos(glm::radians(passTime * 360.0f)) * 40.0f;
    m_lightPos.y = -10.0f + sin(glm::radians(passTime * 360.0f)) * 20.0f;
    m_lightPos.z = 15.0f + sin(glm::radians(passTime * 360.0f)) * 5.0f;
}

void ShadowQuality::updateShadowMapMVP()
{
    // Matrix from light's point of view
//    glm::mat4 depthProjectionMatrix = glm::perspective(glm::radians(m_lightFOV), 1.0f, m_zNear, m_zFar);
//    glm::mat4 depthViewMatrix = glm::lookAt(m_lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
//    glm::mat4 depthModelMatrix = glm::mat4(1.0f);
//    m_shadowUniformMvp.shadowMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
    
    glm::mat4 mvp;
    mvp[0][0] = 0.0146112442f;
    mvp[1][0] = 0.0146112442f;
    mvp[2][0] = 0;
    mvp[3][0] = 0.00356149673f;
    
    mvp[0][1] = -0.0142962011f;
    mvp[1][1] =  0.0142962011f;
    mvp[2][1] =  0.0285924021f;
    mvp[3][1] =  0.00366449356f;
    
    mvp[0][2] = -0.00722600846f;
    mvp[1][2] =  0.00722600846f;
    mvp[2][2] = -0.00722600846f;
    mvp[3][2] =  0.496343613f;
    
    mvp[0][3] = 0;
    mvp[1][3] = 0;
    mvp[2][3] = 0;
    mvp[3][3] = 1;
    
    m_shadowUniformMvp.shadowMVP = mvp;
    Tools::mapMemory(m_shadowUniformMemory, sizeof(ShadowUniform), &m_shadowUniformMvp);
}

void ShadowQuality::updateUniformMVP()
{
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::mat4(1.0f);
    mvp.shadowMapMvp = m_shadowUniformMvp.shadowMVP;
    mvp.lightPos = glm::vec4(m_lightPos, 1.0f);
    mvp.zNear = m_zNear;
    mvp.zFar = m_zFar;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void ShadowQuality::createOtherBuffer()
{
    //color
    Tools::createImageAndMemoryThenBind(m_offscreenColorFormat, m_shadowMapWidth, m_shadowMapHeight, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_offscreenColorImage, m_offscreenColorMemory);
    
    Tools::createImageView(m_offscreenColorImage, m_offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_offscreenColorImageView);
    
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_offscreenColorSampler);
    
    
    // depth
    Tools::createImageAndMemoryThenBind(m_offscreenDepthFormat, m_shadowMapWidth, m_shadowMapHeight, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_offscreenDepthImage, m_offscreenDepthMemory);
    
    Tools::createImageView(m_offscreenDepthImage, m_offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_offscreenDepthImageView);
    
    // sample
    VkFilter filter = true ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    Tools::createTextureSampler(filter, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_offscreenDepthSampler);
}

void ShadowQuality::createOffscreenRenderPass()
{
    VkAttachmentDescription attachmentDescriptions[2];
    attachmentDescriptions[0] = Tools::getAttachmentDescription(m_offscreenColorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    attachmentDescriptions[1] = Tools::getAttachmentDescription(m_offscreenDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
 
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
    createInfo.attachmentCount = 2;
    createInfo.pAttachments = attachmentDescriptions;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    
    VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_offscreenRenderPass) );
}

void ShadowQuality::createOffscreenFrameBuffer()
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

void ShadowQuality::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
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

//    vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
    m_planeLoader.bindBuffers(commandBuffer);
    m_planeLoader.draw(commandBuffer);
    
    m_sphereLoader.bindBuffers(commandBuffer);
    m_sphereLoader.draw(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);
}


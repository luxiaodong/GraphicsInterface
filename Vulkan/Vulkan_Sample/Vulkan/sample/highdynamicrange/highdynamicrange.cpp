
#include "highdynamicrange.h"

HighDynamicRange::HighDynamicRange(std::string title) : Application(title)
{
}

HighDynamicRange::~HighDynamicRange()
{}

void HighDynamicRange::init()
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

void HighDynamicRange::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -6.0f));
    m_camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void HighDynamicRange::setEnabledFeatures()
{
//    if(m_deviceFeatures.samplerAnisotropy)
//    {
//        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
//    }
}

void HighDynamicRange::clear()
{
    vkDestroyPipeline(m_device, m_skyboxPipeline, nullptr);
    vkDestroyPipeline(m_device, m_objectPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_skyboxDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_skyboxPipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_offscreenFrameBuffer, nullptr);
    
    vkDestroyImageView(m_device, m_offscreenColorImageView[0], nullptr);
    vkDestroyImage(m_device, m_offscreenColorImage[0], nullptr);
    vkFreeMemory(m_device, m_offscreenColorMemory[0], nullptr);
    vkDestroyImageView(m_device, m_offscreenColorImageView[1], nullptr);
    vkDestroyImage(m_device, m_offscreenColorImage[1], nullptr);
    vkFreeMemory(m_device, m_offscreenColorMemory[1], nullptr);
    vkDestroySampler(m_device, m_offscreenColorSample, nullptr);
    vkDestroyImageView(m_device, m_offscreenDepthImageView, nullptr);
    vkDestroyImage(m_device, m_offscreenDepthImage, nullptr);
    vkFreeMemory(m_device, m_offscreenDepthMemory, nullptr);

    vkFreeMemory(m_device, m_skyboxUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_skyboxUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_exposureUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_exposureUniformBuffer, nullptr);
    
    vkDestroyPipeline(m_device, m_compositionPipeline, nullptr);
    
    m_skyboxLoader.clear();
    m_objectLoader.clear();
    m_pTexture->clear();
    delete m_pTexture;
    Application::clear();
}

void HighDynamicRange::prepareVertex()
{
    m_skyboxLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::FlipY);
    m_skyboxLoader.createVertexAndIndexBuffer();
    m_skyboxLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal});
    m_pTexture = Texture::loadTextrue2D(Tools::getTexturePath() +  "hdr/uffizi_cube.ktx", m_graphicsQueue, VK_FORMAT_R16G16B16A16_SFLOAT, TextureCopyRegion::Cube);

    std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
    m_objectLoader.loadFromFile(Tools::getModelPath() + filenames[1], m_graphicsQueue);
    m_objectLoader.createVertexAndIndexBuffer();
    m_objectLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal});
}

void HighDynamicRange::prepareUniform()
{
    // skybox
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_skyboxUniformBuffer, m_skyboxUniformMemory);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.invViewMatrix = glm::inverse(m_camera.m_viewMat);
    Tools::mapMemory(m_skyboxUniformMemory, sizeof(Uniform), &mvp);
    
    uniformSize = sizeof(Exposure);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_exposureUniformBuffer, m_exposureUniformMemory);
    
    Exposure exposure = {};
    exposure.exposure = 1.0f;
    Tools::mapMemory(m_exposureUniformMemory, sizeof(Exposure), &exposure);
}

void HighDynamicRange::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_skyboxDescriptorSetLayout));
        createPipelineLayout(&m_skyboxDescriptorSetLayout, 1, m_skyboxPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout();
    }
}

void HighDynamicRange::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 3;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    
    {
        createDescriptorSet(&m_skyboxDescriptorSetLayout, 1, m_skyboxDescriptorSet);

        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_skyboxUniformBuffer;

        VkDescriptorImageInfo imageInfo = m_pTexture->getDescriptorImageInfo();
        
        VkDescriptorBufferInfo bufferInfo2 = {};
        bufferInfo2.offset = 0;
        bufferInfo2.range = VK_WHOLE_SIZE;
        bufferInfo2.buffer = m_exposureUniformBuffer;

        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        writes[2] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &bufferInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_compositionDescriptorSet);
        
        VkDescriptorImageInfo imageInfo1 = {};
        imageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo1.imageView = m_offscreenColorImageView[0];
        imageInfo1.sampler = m_offscreenColorSample;
        
        VkDescriptorImageInfo imageInfo2 = {};
        imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo2.imageView = m_offscreenColorImageView[0];
        imageInfo2.sampler = m_offscreenColorSample;
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_compositionDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_compositionDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void HighDynamicRange::createGraphicsPipeline()
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

    std::array<VkPipelineColorBlendAttachmentState, 2> colorBlendAttachments;
    colorBlendAttachments[0] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    colorBlendAttachments[1] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
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
    
    int skyboxOrObjectIndex = 0;
    VkSpecializationMapEntry specializationMapEntry;
    specializationMapEntry.constantID = 0;
    specializationMapEntry.size = sizeof(int);
    specializationMapEntry.offset = 0;
    
    VkSpecializationInfo specializationInfo = {};
    specializationInfo.dataSize = sizeof(int);
    specializationInfo.mapEntryCount = 1;
    specializationInfo.pMapEntries = &specializationMapEntry;
    specializationInfo.pData = &skyboxOrObjectIndex;
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/gbuffer.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/gbuffer.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderStages[0].pSpecializationInfo = &specializationInfo;
    shaderStages[1].pSpecializationInfo = &specializationInfo;
    
    createInfo.layout = m_skyboxPipelineLayout;
    createInfo.renderPass = m_offscreenRenderPass;
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthTestEnable = VK_FALSE;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_skyboxPipeline));
    
    createInfo.pVertexInputState = m_objectLoader.getPipelineVertexInputState();
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    skyboxOrObjectIndex = 1;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthTestEnable = VK_TRUE;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_objectPipeline));
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);

    // composition
    VkPipelineVertexInputStateCreateInfo emptyVertexInput = {};
    emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    emptyVertexInput.flags = 0;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    createInfo.pVertexInputState = &emptyVertexInput;
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend2 = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    createInfo.pColorBlendState = &colorBlend2;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/composition.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "hdr/composition.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_compositionPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void HighDynamicRange::updateRenderData()
{
}

void HighDynamicRange::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositionPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_compositionDescriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

//
void HighDynamicRange::createOtherBuffer()
{
    // color0
    Tools::createImageAndMemoryThenBind(m_offscreenColorFormat, m_swapchainExtent.width, m_swapchainExtent.height, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_offscreenColorImage[0], m_offscreenColorMemory[0]);
    Tools::createImageView(m_offscreenColorImage[0], m_offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_offscreenColorImageView[0]);
    
    // color1
    Tools::createImageAndMemoryThenBind(m_offscreenColorFormat, m_swapchainExtent.width, m_swapchainExtent.height, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_offscreenColorImage[1], m_offscreenColorMemory[1]);
    Tools::createImageView(m_offscreenColorImage[1], m_offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_offscreenColorImageView[1]);
    
    // sample
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_offscreenColorSample);
    
    
    // depth
    Tools::createImageAndMemoryThenBind(m_offscreenDepthFormat, m_swapchainExtent.width, m_swapchainExtent.height, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_offscreenDepthImage, m_offscreenDepthMemory);
    
    Tools::createImageView(m_offscreenDepthImage, m_offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_offscreenDepthImageView);
}

void HighDynamicRange::createOffscreenRenderPass()
{
    std::array<VkAttachmentDescription, 3> attachmentDescription;
    attachmentDescription[0] = Tools::getAttachmentDescription(m_offscreenColorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    attachmentDescription[1] = Tools::getAttachmentDescription(m_offscreenColorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    attachmentDescription[2] = Tools::getAttachmentDescription(m_offscreenDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
 
    VkAttachmentReference colorAttachmentReference[2] = {};
    colorAttachmentReference[0].attachment = 0;
    colorAttachmentReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentReference[1].attachment = 1;
    colorAttachmentReference[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 2;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 2;
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

void HighDynamicRange::createOffscreenFrameBuffer()
{
    std::vector<VkImageView> attachments;
    attachments.push_back(m_offscreenColorImageView[0]);
    attachments.push_back(m_offscreenColorImageView[1]);
    attachments.push_back(m_offscreenDepthImageView);
    
    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = m_offscreenRenderPass;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.width = m_swapchainExtent.width;
    createInfo.height = m_swapchainExtent.height;
    createInfo.layers = 1;
    
    VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_offscreenFrameBuffer));
}

void HighDynamicRange::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    std::array<VkClearValue, 3> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[2].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_offscreenRenderPass;
    passBeginInfo.framebuffer = m_offscreenFrameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent = m_swapchainExtent;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    m_skyboxLoader.bindBuffers(commandBuffer);
    m_skyboxLoader.draw(commandBuffer);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_objectPipeline);
    m_objectLoader.bindBuffers(commandBuffer);
    m_objectLoader.draw(commandBuffer);
    
    vkCmdEndRenderPass(commandBuffer);
}


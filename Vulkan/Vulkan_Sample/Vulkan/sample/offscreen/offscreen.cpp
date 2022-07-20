
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
}

void OffScreen::createOtherBuffer()
{
//    Tools::createImageAndMemoryThenBind(m_surfaceFormatKHR.format, m_swapchainExtent.width, m_swapchainExtent.height, 1, 1,
//                                 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
//                                 VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
//                                 m_colorImage, m_colorMemory);
//
//    Tools::createImageView(m_colorImage, m_surfaceFormatKHR.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_colorImageView);
    
//    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
//    Tools::setImageLayout(cmd, m_colorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
//    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
}

void OffScreen::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(&binding, 1);
    createPipelineLayout();
}

void OffScreen::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 1> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
//    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//    poolSizes[1].descriptorCount = 2;
//    poolSizes[1].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
//    poolSizes[1].descriptorCount = 2;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);
    createDescriptorSet(m_descriptorSet);
    
//    VkDescriptorSetAllocateInfo allocInfo = {};
//    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//    allocInfo.descriptorPool = m_descriptorPool;
//    allocInfo.descriptorSetCount = 1;
//    allocInfo.pSetLayouts = &m_readDescriptorSetLayout;
//
//    if( vkAllocateDescriptorSets(m_device, &allocInfo, &m_readDescriptorSet) != VK_SUCCESS)
//    {
//        throw std::runtime_error("failed to allocate descriptorSets!");
//    }
    
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Uniform);
    bufferInfo.buffer = m_uniformBuffer;
    
    VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
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

    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "offscreen/phong.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "offscreen/phong.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

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
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    m_dragonLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_dragonLoader.draw(commandBuffer);
    
    // pass 1
//    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_readPipeline);
//    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_readPipelineLayout, 0, 1, &m_readDescriptorSet, 0, NULL);
//    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

//
//void OffScreen::createAttachmentDescription()
//{
//    // Swap chain image color attachment
//    // Will be transitioned to present layout
//    VkAttachmentDescription surfaceAttachmentDescription = Tools::getAttachmentDescription(m_surfaceFormatKHR.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
//
//    // Input attachments
//    // These will be written in the first subpass, transitioned to input attachments
//    // and then read in the secod subpass
//    VkAttachmentDescription colorAttachmentDescription = Tools::getAttachmentDescription(m_surfaceFormatKHR.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//
//    // Depth
//    VkAttachmentDescription depthAttachmentDescription = Tools::getAttachmentDescription(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
//
//    m_attachmentDescriptions.push_back(surfaceAttachmentDescription);
//    m_attachmentDescriptions.push_back(colorAttachmentDescription);
//    m_attachmentDescriptions.push_back(depthAttachmentDescription);
//}

//void OffScreen::createRenderPass()
//{
//    // pass 1
//    VkAttachmentReference colorAttachmentReference = {};
//    colorAttachmentReference.attachment = 1;
//    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//
//    VkAttachmentReference depthAttachmentReference = {};
//    depthAttachmentReference.attachment = 2;
//    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//
//    // pass 2
//    VkAttachmentReference colorSwapChainAttachmentReference = {};
//    colorSwapChainAttachmentReference.attachment = 0;
//    colorSwapChainAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//
//    VkAttachmentReference inputAttachmentReference[2];
//    inputAttachmentReference[0].attachment = 1;
//    inputAttachmentReference[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//    inputAttachmentReference[1].attachment = 2;
//    inputAttachmentReference[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//    VkSubpassDescription subpassDescription[2] = {};
//    subpassDescription[0].flags = 0;
//    subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//    subpassDescription[0].inputAttachmentCount = 0;
//    subpassDescription[0].pInputAttachments = nullptr;
//    subpassDescription[0].colorAttachmentCount = 1;
//    subpassDescription[0].pColorAttachments = &colorAttachmentReference;
//    subpassDescription[0].pResolveAttachments = nullptr;
//    subpassDescription[0].pDepthStencilAttachment = &depthAttachmentReference;
//    subpassDescription[0].preserveAttachmentCount = 0;
//    subpassDescription[0].pPreserveAttachments = nullptr;
//
//    subpassDescription[1].flags = 0;
//    subpassDescription[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//    subpassDescription[1].inputAttachmentCount = 2;
//    subpassDescription[1].pInputAttachments = inputAttachmentReference;
//    subpassDescription[1].colorAttachmentCount = 1;
//    subpassDescription[1].pColorAttachments = &colorSwapChainAttachmentReference;
//    subpassDescription[1].pResolveAttachments = nullptr;
//    subpassDescription[1].pDepthStencilAttachment = nullptr;
//    subpassDescription[1].preserveAttachmentCount = 0;
//    subpassDescription[1].pPreserveAttachments = nullptr;
//
//    VkSubpassDependency dependencies[1] = {};
////    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
////    dependencies[0].dstSubpass = 0;
////    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
////    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
////    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
////    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
////    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
//
//    // This dependency transitions the input attachment from color attachment to shader read
//    dependencies[0].srcSubpass = 0;
//    dependencies[0].dstSubpass = 1;
//    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//    dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
//    dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
//
////    dependencies[2].srcSubpass = 0;
////    dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
////    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
////    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
////    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
////    dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
////    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
//
//    VkRenderPassCreateInfo createInfo = {};
//    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
//    createInfo.flags = 0;
//    createInfo.attachmentCount = static_cast<uint32_t>(m_attachmentDescriptions.size());
//    createInfo.pAttachments = m_attachmentDescriptions.data();
//    createInfo.subpassCount = 2;
//    createInfo.pSubpasses = subpassDescription;
//    createInfo.dependencyCount = 1;
//    createInfo.pDependencies = dependencies;
//
//    if( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass) != VK_SUCCESS )
//    {
//        throw std::runtime_error("failed to create renderpass!");
//    }
//}



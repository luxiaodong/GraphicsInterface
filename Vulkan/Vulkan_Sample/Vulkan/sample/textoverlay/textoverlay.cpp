
#include "textoverlay.h"

Textoverlay::Textoverlay(std::string title) : Application(title)
{

}

Textoverlay::~Textoverlay()
{}

void Textoverlay::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
    
    prepareText();
}

void Textoverlay::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
    m_camera.setRotation(glm::vec3(-25.0f, -0.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void Textoverlay::setEnabledFeatures()
{
//    if( m_deviceFeatures.fillModeNonSolid )
//    {
//        m_deviceEnabledFeatures.fillModeNonSolid = VK_TRUE;
//        if( m_deviceFeatures.wideLines )
//        {
//            m_deviceEnabledFeatures.wideLines = VK_TRUE;
//        }
//    }
}

void Textoverlay::clear()
{
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    m_gltfLoader.clear();

    m_pText->clear();
    delete m_pText;
    Application::clear();
}

void Textoverlay::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    
    m_gltfLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue, flags);
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV});
}

void Textoverlay::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);

    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void Textoverlay::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(&binding, 1);
    createPipelineLayout();
}

void Textoverlay::prepareDescriptorSetAndWrite()
{
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;
    createDescriptorPool(&poolSize, 1, 1);
    createDescriptorSet(m_descriptorSet);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Uniform);
    bufferInfo.buffer = m_uniformBuffer;

    VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void Textoverlay::createGraphicsPipeline()
{
//    VkPipelineVertexInputStateCreateInfo vertexInput = {};
//    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindDes.size());
//    vertexInput.pVertexBindingDescriptions = m_vertexInputBindDes.data();
//    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttrDes.size());
//    vertexInput.pVertexAttributeDescriptions = m_vertexInputAttrDes.data();

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

    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "textoverlay/mesh.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "textoverlay/mesh.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void Textoverlay::updateRenderData()
{
    
}

void Textoverlay::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_gltfLoader.bindBuffers(commandBuffer);

    // phong
    viewport.width = (float)m_swapchainExtent.width;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_gltfLoader.draw(commandBuffer);
    
    m_pText->recordRenderCommand(commandBuffer);
}

void Textoverlay::prepareText()
{
    m_pText = new Text(m_surfaceFormatKHR.format, m_depthFormat, m_swapchainExtent.width, m_swapchainExtent.height, Tools::getShaderPath() + "textoverlay/text.vert.spv", Tools::getShaderPath() + "textoverlay/text.frag.spv");
    m_pText->init();

    glm::vec3 projected = glm::project(glm::vec3(0.0f), m_camera.m_viewMat, m_camera.m_projMat, glm::vec4(0, 0, (float)m_swapchainExtent.width, (float)m_swapchainExtent.height));

    m_pText->begin();
    m_pText->addString("A Cube", 5.0f, 5.0f, TextAlign::Center);
    m_pText->end();
}

void Textoverlay::createOtherRenderPass(const VkFramebuffer& frameBuffer)
{
//    m_pText->updateCommandBuffers(frameBuffer);
}


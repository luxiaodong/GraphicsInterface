
#include "imgui.h"

ImGUI::ImGUI(std::string title) : Application(title)
{
}

ImGUI::~ImGUI()
{}

void ImGUI::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();

}

void ImGUI::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -0.48f));
    m_camera.setRotation(glm::vec3(4.5f, -380.0f, 0.0f));
    m_camera.setPerspective(45.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void ImGUI::setEnabledFeatures()
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

void ImGUI::clear()
{
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    m_backgroundLoader.clear();
    m_gameobjectLoader.clear();
    m_vulkanlogoLoader.clear();
    Application::clear();
}

void ImGUI::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    
    m_backgroundLoader.loadFromFile(Tools::getModelPath() + "vulkanscenebackground.gltf", m_graphicsQueue, flags);
    m_gameobjectLoader.loadFromFile(Tools::getModelPath() + "vulkanscenemodels.gltf", m_graphicsQueue, flags);
    m_vulkanlogoLoader.loadFromFile(Tools::getModelPath() + "vulkanscenelogos.gltf", m_graphicsQueue, flags);

    m_backgroundLoader.createVertexAndIndexBuffer();
    m_gameobjectLoader.createVertexAndIndexBuffer();
    m_vulkanlogoLoader.createVertexAndIndexBuffer();
    
    m_vulkanlogoLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
}

void ImGUI::prepareUniform()
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

void ImGUI::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(&binding, 1);
    createPipelineLayout();
}

void ImGUI::prepareDescriptorSetAndWrite()
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

void ImGUI::createGraphicsPipeline()
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

    createInfo.pVertexInputState = m_vulkanlogoLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;

    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "imgui/scene.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "imgui/scene.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void ImGUI::updateRenderData()
{
    
}

void ImGUI::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_backgroundLoader.bindBuffers(commandBuffer);
    m_backgroundLoader.draw(commandBuffer);
    m_gameobjectLoader.bindBuffers(commandBuffer);
    m_gameobjectLoader.draw(commandBuffer);
    m_vulkanlogoLoader.bindBuffers(commandBuffer);
    m_vulkanlogoLoader.draw(commandBuffer);
}



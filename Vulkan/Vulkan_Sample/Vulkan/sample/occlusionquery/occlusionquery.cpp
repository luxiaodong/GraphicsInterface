
#include "occlusionquery.h"
#include <stdlib.h>
#include <random>

OcclusionQuery::OcclusionQuery(std::string title) : Application(title)
{
}

OcclusionQuery::~OcclusionQuery()
{}

void OcclusionQuery::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void OcclusionQuery::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -7.5f));
    m_camera.setRotation(glm::vec3(0.0f, -123.75f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void OcclusionQuery::setEnabledFeatures()
{
}

void OcclusionQuery::clear()
{
    vkDestroyBuffer(m_device, m_sphereBuffer, nullptr);
    vkFreeMemory(m_device, m_sphereMemory, nullptr);
    vkDestroyBuffer(m_device, m_teapotBuffer, nullptr);
    vkFreeMemory(m_device, m_teapotMemory, nullptr);
    vkDestroyBuffer(m_device, m_occluderBuffer, nullptr);
    vkFreeMemory(m_device, m_occluderMemory, nullptr);
    
    vkDestroyPipeline(m_device, m_simplePipeline, nullptr);
    vkDestroyPipeline(m_device, m_solidPipeline, nullptr);
    vkDestroyPipeline(m_device, m_occluderPipeline, nullptr);
    
    vkDestroyQueryPool(m_device, m_queryPool, nullptr);
    
    m_planeLoader.clear();
    m_teapotLoader.clear();
    m_sphereLoader.clear();
    Application::clear();
}

void OcclusionQuery::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    
    m_planeLoader.loadFromFile(Tools::getModelPath() + "plane_z.gltf", m_graphicsQueue, flags);
    m_planeLoader.createVertexAndIndexBuffer();
    m_planeLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
    
    m_teapotLoader.loadFromFile(Tools::getModelPath() + "teapot.gltf", m_graphicsQueue, flags);
    m_teapotLoader.createVertexAndIndexBuffer();
    m_teapotLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
    
    m_sphereLoader.loadFromFile(Tools::getModelPath() + "sphere.gltf", m_graphicsQueue, flags);
    m_sphereLoader.createVertexAndIndexBuffer();
    m_sphereLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
    
    prepareOcclusionQuery();
}

void OcclusionQuery::prepareOcclusionQuery()
{
    VkQueryPoolCreateInfo queryPoolInfo = {};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
    queryPoolInfo.queryCount = 2;
    VK_CHECK_RESULT(vkCreateQueryPool(m_device, &queryPoolInfo, NULL, &m_queryPool));
}

void OcclusionQuery::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_sphereBuffer, m_sphereMemory);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_teapotBuffer, m_teapotMemory);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_occluderBuffer, m_occluderMemory);

}

void OcclusionQuery::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 1> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void OcclusionQuery::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 1> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 3;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 3);

    {
        createDescriptorSet(m_sphereDescriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_sphereBuffer;
        
        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_sphereDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_teapotDescriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_teapotBuffer;
        
        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_teapotDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_occluderBuffer;
        
        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void OcclusionQuery::createGraphicsPipeline()
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
    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    createInfo.pVertexInputState = m_sphereLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "occlusionquery/mesh.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "occlusionquery/mesh.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_solidPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "occlusionquery/simple.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "occlusionquery/simple.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_simplePipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // Enable blending
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "occlusionquery/occluder.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "occlusionquery/occluder.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_occluderPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void OcclusionQuery::updateRenderData()
{
    VkDeviceSize uniformSize = sizeof(Uniform);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.visible = true;
    
    // Sphere
    mvp.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 3.0f));
    mvp.color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    mvp.visible = (m_passedSamples[1] > 0) ? 1.0f : 0.0f;
    Tools::mapMemory(m_sphereMemory, uniformSize, &mvp);
    
    // Teapot
    mvp.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    mvp.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    mvp.visible = (m_passedSamples[0] > 0) ? 1.0f : 0.0f;
    Tools::mapMemory(m_teapotMemory, uniformSize, &mvp);
    
    // Occluder
    mvp.modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(6.0f));
    mvp.color = glm::vec4(0.0f, 0.0f, 1.0f, 0.5f);
    mvp.visible = true;
    Tools::mapMemory(m_occluderMemory, uniformSize, &mvp);
}

void OcclusionQuery::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    // Occlusion pass
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_occluderPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_planeLoader.bindBuffers(commandBuffer);
    m_planeLoader.draw(commandBuffer);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_teapotDescriptorSet, 0, nullptr);
    vkCmdBeginQuery(commandBuffer, m_queryPool, 0, 0);
    m_teapotLoader.bindBuffers(commandBuffer);
    m_teapotLoader.draw(commandBuffer);
    vkCmdEndQuery(commandBuffer, m_queryPool, 0);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_sphereDescriptorSet, 0, nullptr);
    vkCmdBeginQuery(commandBuffer, m_queryPool, 1, 0);
    m_sphereLoader.bindBuffers(commandBuffer);
    m_sphereLoader.draw(commandBuffer);
    vkCmdEndQuery(commandBuffer, m_queryPool, 1);

    // 加了下面的代码,导致上面的查询数值变化. 不理解?
    
    // clear color && depth
    VkClearAttachment clearAttachments[2] = {};
    clearAttachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachments[0].clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearAttachments[0].colorAttachment = 0;
    clearAttachments[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clearAttachments[1].clearValue.depthStencil = { 1.0f, 0 };

    VkClearRect clearRect = {};
    clearRect.layerCount = 1;
    clearRect.rect.offset = { 0, 0 };
    clearRect.rect.extent = { m_swapchainExtent.width, m_swapchainExtent.height};
    vkCmdClearAttachments(commandBuffer,2, clearAttachments, 1, &clearRect);

    // render object
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_solidPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_teapotDescriptorSet, 0, nullptr);
    m_teapotLoader.bindBuffers(commandBuffer);
    m_teapotLoader.draw(commandBuffer);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_sphereDescriptorSet, 0, nullptr);
    m_sphereLoader.bindBuffers(commandBuffer);
    m_sphereLoader.draw(commandBuffer);

    // Occluder
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_occluderPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_planeLoader.bindBuffers(commandBuffer);
    m_planeLoader.draw(commandBuffer);
}

void OcclusionQuery::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    vkCmdResetQueryPool(commandBuffer, m_queryPool, 0, 2);
}

void OcclusionQuery::queueResult()
{
    // We use vkGetQueryResults to copy the results into a host visible buffer
    // Store results a 64 bit values and wait until the results have been finished
    // If you don't want to wait, you can use VK_QUERY_RESULT_WITH_AVAILABILITY_BIT
    // which also returns the state of the result (ready) in the result
    vkGetQueryPoolResults(m_device, m_queryPool, 0, 2, sizeof(m_passedSamples), m_passedSamples, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    std::cout << "Teapot : " << m_passedSamples[0] << std::endl;
    std::cout << "Sphere : " << m_passedSamples[1] << std::endl;
}

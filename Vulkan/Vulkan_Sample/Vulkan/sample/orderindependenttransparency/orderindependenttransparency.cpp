
#include "orderindependenttransparency.h"

#define TOTAL_NODE_COUNT 16

OrderIndependentTransparency::OrderIndependentTransparency(std::string title) : Application(title)
{

}

OrderIndependentTransparency::~OrderIndependentTransparency()
{}

void OrderIndependentTransparency::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGeometryRenderPass();
    createGeometryFrameBuffer();
    
    createGraphicsPipeline();
}

void OrderIndependentTransparency::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
    m_camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)(m_width/3.0f) / (float)m_height, 0.1f, 256.0f);
}

void OrderIndependentTransparency::setEnabledFeatures()
{
    if( m_deviceFeatures.fragmentStoresAndAtomics )
    {
        m_deviceEnabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
    }
}

void OrderIndependentTransparency::clear()
{
    vkDestroyPipeline(m_device, m_geometryPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_geometryPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_geometryDescriptorSetLayout, nullptr);
    vkDestroyRenderPass(m_device, m_geometryRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_geometryFrameBuffer, nullptr);
    vkDestroyImage(m_device, m_geometryImage, nullptr);
    vkDestroyImageView(m_device, m_geometryImageView, nullptr);
    vkFreeMemory(m_device, m_geometryMemory, nullptr);
    
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_geometryUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_geometryUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_nodeUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_nodeUniformBuffer, nullptr);
    
    vkDestroyPipeline(m_device, m_pipeline, nullptr);

    m_cubeLoader.clear();
    m_sphereLoader.clear();
    Application::clear();
}

void OrderIndependentTransparency::prepareVertex()
{
    m_cubeLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::FlipY);
    m_cubeLoader.createVertexAndIndexBuffer();
    m_cubeLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position});
    
    m_sphereLoader.loadFromFile(Tools::getModelPath() + "sphere.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::FlipY);
    m_sphereLoader.createVertexAndIndexBuffer();
    m_sphereLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position});
}

void OrderIndependentTransparency::prepareUniform()
{
    // uniform
    VkDeviceSize uniformSize = sizeof(Uniform);

    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    
    // geometryBuffer
    GeometryBuffer geometryBuffer = {};
    geometryBuffer.count = 0;
    geometryBuffer.maxNodeCount = TOTAL_NODE_COUNT * m_swapchainExtent.width * m_swapchainExtent.height;
    
    VkDeviceSize bufferSize = sizeof(GeometryBuffer);
    VkBuffer stageBuffer;
    VkDeviceMemory stageMemory;
    Tools::createBufferAndMemoryThenBind(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stageBuffer, stageMemory);
    Tools::mapMemory(stageMemory, bufferSize, &geometryBuffer);
    Tools::createBufferAndMemoryThenBind(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_geometryUniformBuffer, m_geometryUniformMemory);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    VkBufferCopy bufferCopy = {};
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = 0;
    bufferCopy.size = bufferSize;
    vkCmdCopyBuffer(cmd, stageBuffer, m_geometryUniformBuffer, 1, &bufferCopy);
    
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    vkFreeMemory(m_device, stageMemory, nullptr);
    vkDestroyBuffer(m_device, stageBuffer, nullptr);
    
    // node
    VkDeviceSize nodeBufferSize = sizeof(Node) * geometryBuffer.maxNodeCount;
    Tools::createBufferAndMemoryThenBind(nodeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                         m_nodeUniformBuffer, m_nodeUniformMemory);
}

void OrderIndependentTransparency::prepareDescriptorSetLayoutAndPipelineLayout()
{
    m_pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    m_pushConstantRange.offset = 0;
    m_pushConstantRange.size = sizeof(PushConstantData);
    
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        bindings[3] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_geometryDescriptorSetLayout));
        createPipelineLayout(&m_geometryDescriptorSetLayout, 1, m_geometryPipelineLayout, &m_pushConstantRange, 1);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout();
    }
}

void OrderIndependentTransparency::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 3> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 3;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[2].descriptorCount = 2;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    
    {
        createDescriptorSet(&m_geometryDescriptorSetLayout, 1, m_geometryDescriptorSet);
        std::array<VkWriteDescriptorSet, 4> writes = {};
        
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_uniformBuffer;
        
        VkDescriptorBufferInfo bufferInfo2 = {};
        bufferInfo2.offset = 0;
        bufferInfo2.range = VK_WHOLE_SIZE;
        bufferInfo2.buffer = m_geometryUniformBuffer;
        
        VkDescriptorImageInfo imageInfo3 = {};
        imageInfo3.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo3.imageView = m_geometryImageView;
        imageInfo3.sampler = VK_NULL_HANDLE;
        
        VkDescriptorBufferInfo bufferInfo4 = {};
        bufferInfo4.offset = 0;
        bufferInfo4.range = VK_WHOLE_SIZE;
        bufferInfo4.buffer = m_nodeUniformBuffer;
        
        writes[0] = Tools::getWriteDescriptorSet(m_geometryDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_geometryDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &bufferInfo2);
        writes[2] = Tools::getWriteDescriptorSet(m_geometryDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &imageInfo3);
        writes[3] = Tools::getWriteDescriptorSet(m_geometryDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &bufferInfo4);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        
        VkDescriptorImageInfo imageInfo3 = {};
        imageInfo3.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo3.imageView = m_geometryImageView;
        imageInfo3.sampler = VK_NULL_HANDLE;
        
        VkDescriptorBufferInfo bufferInfo4 = {};
        bufferInfo4.offset = 0;
        bufferInfo4.range = VK_WHOLE_SIZE;
        bufferInfo4.buffer = m_nodeUniformBuffer;
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &imageInfo3);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &bufferInfo4);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

}

void OrderIndependentTransparency::createGraphicsPipeline()
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

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, 0);
    VkPipelineColorBlendStateCreateInfo colorBlend0 = Tools::getPipelineColorBlendStateCreateInfo(0, nullptr);
    VkPipelineColorBlendStateCreateInfo colorBlend1 = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

//    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

    createInfo.pVertexInputState = m_cubeLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend0;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;

    //geometry
    createInfo.renderPass = m_geometryRenderPass;
    createInfo.layout = m_geometryPipelineLayout;
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "oit/geometry.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "oit/geometry.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_geometryPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
 
    //color
    createInfo.renderPass = m_renderPass;
    createInfo.layout = m_pipelineLayout;
    createInfo.pColorBlendState = &colorBlend1;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "oit/color.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "oit/color.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void OrderIndependentTransparency::updateRenderData()
{
    Uniform mvp = {};
    mvp.modelMatrix = m_camera.m_viewMat;
    mvp.projectionMatrix = m_camera.m_projMat;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void OrderIndependentTransparency::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

//
void OrderIndependentTransparency::createOtherBuffer()
{
    Tools::createImageAndMemoryThenBind(m_geometryFormat, m_swapchainExtent.width, m_swapchainExtent.height, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_geometryImage, m_geometryMemory);
//
    Tools::createImageView(m_geometryImage, m_geometryFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_geometryImageView);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Tools::setImageLayout(cmd, m_geometryImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);

}

void OrderIndependentTransparency::createGeometryRenderPass()
{
    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    
    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.attachmentCount = 0;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = 0;
    
    VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_geometryRenderPass) );
}

void OrderIndependentTransparency::createGeometryFrameBuffer()
{
    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = m_geometryRenderPass;
    createInfo.attachmentCount = 0;
    createInfo.width = m_swapchainExtent.width;
    createInfo.height = m_swapchainExtent.height;
    createInfo.layers = 1;
    
    VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_geometryFrameBuffer));
}

void OrderIndependentTransparency::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    VkClearColorValue clearColor;
    clearColor.uint32[0] = 0xffffffff;
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
    vkCmdClearColorImage(commandBuffer, m_geometryImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);
    // Clear previous geometry pass data
    vkCmdFillBuffer(commandBuffer, m_geometryUniformBuffer, 0, sizeof(uint32_t), 0);
    
    // We need a barrier to make sure all writes are finished before starting to write again
    VkMemoryBarrier memoryBarrier {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_geometryRenderPass;
    passBeginInfo.framebuffer = m_geometryFrameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent = m_swapchainExtent;
    passBeginInfo.clearValueCount = 0;
    passBeginInfo.pClearValues = nullptr;
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geometryPipelineLayout, 0, 1, &m_geometryDescriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geometryPipeline);
    m_sphereLoader.bindBuffers(commandBuffer);
    
    PushConstantData pushdata = {};
    pushdata.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
    for (int32_t x = 0; x < 5; x++)
    {
        for (int32_t y = 0; y < 5; y++)
        {
            for (int32_t z = 0; z < 5; z++)
            {
                glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x - 2, y - 2, z - 2));
                glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));
                pushdata.model = T * S;
                vkCmdPushConstants(commandBuffer, m_geometryPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData), &pushdata);
                m_sphereLoader.draw(commandBuffer);
            }
        }
    }
    
    m_cubeLoader.bindBuffers(commandBuffer);
    pushdata.color = glm::vec4(0.0f, 0.0f, 1.0f, 0.5f);
    for (uint32_t x = 0; x < 2; x++)
    {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f * x - 1.5f, 0.0f, 0.0f));
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f));
        pushdata.model = T * S;
        vkCmdPushConstants(commandBuffer, m_geometryPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData), &pushdata);
        m_cubeLoader.draw(commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);
    
    // Make a pipeline barrier to guarantee the geometry pass is done
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);
    
    {
        // We need a barrier to make sure all writes are finished before starting to write again
        VkMemoryBarrier memoryBarrier {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    }
    
}



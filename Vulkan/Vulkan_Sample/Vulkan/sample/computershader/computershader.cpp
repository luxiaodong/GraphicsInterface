
#include "computershader.h"

ComputerShader::ComputerShader(std::string title) : Application(title)
{
}

ComputerShader::~ComputerShader()
{}

void ComputerShader::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareTextureTarget();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createComputePipeline();
    createGraphicsPipeline();
}

void ComputerShader::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void ComputerShader::clear()
{
    vkDestroyPipeline(m_device, m_computerPipeline, nullptr);
    vkDestroyCommandPool(m_device, m_computerCommandPool, nullptr);
    vkDestroyPipelineLayout(m_device, m_computerPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_computerDescriptorSetLayout, nullptr);
    vkDestroySemaphore(m_device, m_computerSemaphore, nullptr);
    vkDestroySemaphore(m_device, m_graphicsSemaphore, nullptr);
    
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_vertexMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexMemory, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    
    m_pTexture->clear();
    m_pComputerTarget->clear();
    delete m_pTexture;
    delete m_pComputerTarget;
    Application::clear();
}

void ComputerShader::prepareVertex()
{
    std::vector<Vertex> vertexs =
    {
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f } },
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } }
    };

    std::vector<uint32_t> indexs = {0, 1, 2, 2, 3, 0};
    
    m_vertexInputBindDes.clear();
    m_vertexInputBindDes.push_back(Tools::getVertexInputBindingDescription(0, sizeof(Vertex)));
    
    m_vertexInputAttrDes.clear();
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)));
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)));

    VkDeviceSize vertexSize = vertexs.size() * sizeof(Vertex);
    VkDeviceSize indexSize = indexs.size() * sizeof(uint32_t);
    
    VkBuffer vertexStageBuffer;
    VkDeviceMemory vertexStageMemory;
    Tools::createBufferAndMemoryThenBind(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         vertexStageBuffer, vertexStageMemory);
    Tools::mapMemory(vertexStageMemory, vertexSize, vertexs.data());
    Tools::createBufferAndMemoryThenBind(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexMemory);
    
    VkBuffer indexStageBuffer;
    VkDeviceMemory indexStageMemory;
    Tools::createBufferAndMemoryThenBind(indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         indexStageBuffer, indexStageMemory);
    Tools::mapMemory(indexStageMemory, indexSize, indexs.data());
    Tools::createBufferAndMemoryThenBind(indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexMemory);
    

    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    VkBufferCopy vertexCopy = {};
    vertexCopy.srcOffset = 0;
    vertexCopy.dstOffset = 0;
    vertexCopy.size = vertexSize;
    vkCmdCopyBuffer(cmd, vertexStageBuffer, m_vertexBuffer, 1, &vertexCopy);
    
    VkBufferCopy indexCopy = {};
    indexCopy.srcOffset = 0;
    indexCopy.dstOffset = 0;
    indexCopy.size = indexSize;
    vkCmdCopyBuffer(cmd, indexStageBuffer, m_indexBuffer, 1, &indexCopy);
    
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    
    vkFreeMemory(m_device, vertexStageMemory, nullptr);
    vkDestroyBuffer(m_device, vertexStageBuffer, nullptr);
    vkFreeMemory(m_device, indexStageMemory, nullptr);
    vkDestroyBuffer(m_device, indexStageBuffer, nullptr);
}

void ComputerShader::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void ComputerShader::prepareTextureTarget()
{
    m_pTexture = Texture::loadTextrue2D(Tools::getTexturePath() +  "vulkan_11_rgba.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion::Nothing, VK_IMAGE_LAYOUT_GENERAL);

    m_pComputerTarget = new Texture();
    m_pComputerTarget->m_fromat = VK_FORMAT_R8G8B8A8_UNORM;
    m_pComputerTarget->m_imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    m_pComputerTarget->m_width = m_pTexture->m_width;
    m_pComputerTarget->m_height = m_pTexture->m_height;
    m_pComputerTarget->m_layerCount = 1;
    m_pComputerTarget->m_mipLevels = 1;

    Tools::createImageAndMemoryThenBind(m_pComputerTarget->m_fromat, m_pComputerTarget->m_width, m_pComputerTarget->m_height, m_pComputerTarget->m_mipLevels, m_pComputerTarget->m_layerCount, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_pComputerTarget->m_image, m_pComputerTarget->m_imageMemory);

    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    Tools::setImageLayout(cmd, m_pComputerTarget->m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    vkDeviceWaitIdle(m_device);

    Tools::createImageView(m_pComputerTarget->m_image, m_pComputerTarget->m_fromat, VK_IMAGE_ASPECT_COLOR_BIT, m_pComputerTarget->m_mipLevels, m_pComputerTarget->m_layerCount, m_pComputerTarget->m_imageView);
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, m_pComputerTarget->m_mipLevels, m_pComputerTarget->m_sampler);
}

void ComputerShader::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), 2);
        vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_computerDescriptorSetLayout);
        createPipelineLayout(&m_computerDescriptorSetLayout, 1, m_computerPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout();
    }
}

void ComputerShader::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 3> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[2].descriptorCount = 2;

    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 3);
    
    {
        createDescriptorSet(&m_descriptorSetLayout, 1, m_preComputerDescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pTexture->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_preComputerDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_preComputerDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(&m_descriptorSetLayout, 1, m_postComputerDescriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pComputerTarget->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_postComputerDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_postComputerDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(&m_computerDescriptorSetLayout, 1, m_computerDescriptorSet);
        
        VkDescriptorImageInfo imageInfo1 = {};
        imageInfo1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo1.imageView = m_pTexture->m_imageView;
        imageInfo1.sampler = VK_NULL_HANDLE;
        
        VkDescriptorImageInfo imageInfo2 = {};
        imageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo2.imageView = m_pComputerTarget->m_imageView;
        imageInfo2.sampler = VK_NULL_HANDLE;
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_computerDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &imageInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_computerDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &imageInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void ComputerShader::createComputePipeline()
{
    {
        std::vector<std::string> shaderNames = { "emboss", "edgedetect", "sharpen" };
        VkShaderModule compModule = Tools::createShaderModule( Tools::getShaderPath() + "computeshader/" + shaderNames[0] + ".comp.spv");
        VkPipelineShaderStageCreateInfo stageInfo = Tools::getPipelineShaderStageCreateInfo(compModule, VK_SHADER_STAGE_COMPUTE_BIT);
        
        VkComputePipelineCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        createInfo.stage = stageInfo;
        createInfo.layout = m_computerPipelineLayout;
        VK_CHECK_RESULT(vkCreateComputePipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_computerPipeline));
        vkDestroyShaderModule(m_device, compModule, nullptr);
    }

    {
        VkCommandPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = m_familyIndices.computerFamily.value();
        VK_CHECK_RESULT(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_computerCommandPool));
    }

    // Semaphore for compute & graphics sync
    {
        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.flags = 0;
        VK_CHECK_RESULT(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_graphicsSemaphore));
        
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_graphicsSemaphore;
        VK_CHECK_RESULT(vkQueueSubmit(m_computerQueue, 1, &submitInfo, VK_NULL_HANDLE));
        VK_CHECK_RESULT(vkQueueWaitIdle(m_computerQueue));
    }

    {
        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.flags = 0;
        VK_CHECK_RESULT(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_computerSemaphore));
    }

    // command buffer
    {
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_computerCommandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &allocateInfo, &m_computerCommandBuffer));
        
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        VK_CHECK_RESULT(vkBeginCommandBuffer(m_computerCommandBuffer, &beginInfo));
        
        vkCmdBindPipeline(m_computerCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computerPipeline);
        vkCmdBindDescriptorSets(m_computerCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computerPipelineLayout, 0, 1, &m_computerDescriptorSet, 0, nullptr);
        vkCmdDispatch(m_computerCommandBuffer, m_pComputerTarget->m_width/16, m_pComputerTarget->m_height/16, 1);
        vkEndCommandBuffer(m_computerCommandBuffer);
    }
}

void ComputerShader::createGraphicsPipeline()
{
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "computeshader/texture.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "computeshader/texture.frag.spv");
    
    VkPipelineShaderStageCreateInfo vertShader = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    VkPipelineShaderStageCreateInfo fragShader = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindDes.size());
    vertexInput.pVertexBindingDescriptions = m_vertexInputBindDes.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttrDes.size());
    vertexInput.pVertexAttributeDescriptions = m_vertexInputAttrDes.data();

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
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vertShader);
    shaderStages.push_back(fragShader);
    
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(shaderStages, m_pipelineLayout, m_renderPass);
    
    createInfo.pVertexInputState = &vertexInput;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void ComputerShader::updateRenderData()
{
    
}

void ComputerShader::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
//    VkViewport viewport = Tools::getViewport(-m_swapchainExtent.width*0.25, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // left
    viewport.x = -(float)m_swapchainExtent.width*0.25;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_preComputerDescriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
    
    // right
    viewport.x = (float)m_swapchainExtent.width*0.25;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_postComputerDescriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
}

void ComputerShader::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    // Image memory barrier to make sure that compute shader writes are finished before sampling from the texture
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = m_pComputerTarget->m_image;
    imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier( commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

void ComputerShader::render()
{
    updateRenderData();
    
    if(m_pUi)
    {
        m_pUi->updateRenderData();
    }
    
    // fence需要手动重置为未发出的信号
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
    
    {
        // 这里放computer command
        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_computerCommandBuffer;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_graphicsSemaphore;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_computerSemaphore;
        submitInfo.pWaitDstStageMask = &waitStageMask;
        VK_CHECK_RESULT(vkQueueSubmit(m_computerQueue, 1, &submitInfo, VK_NULL_HANDLE));
    }

    vkAcquireNextImageKHR(m_device, m_swapchainKHR, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);
    
    VkCommandBuffer commandBuffer = m_commandBuffers[m_imageIndex];
    beginRenderCommandAndPass(commandBuffer, m_imageIndex);
    recordRenderCommand(commandBuffer);
    
    if(m_pUi)
    {
        m_pUi->recordRenderCommand(commandBuffer);
    }
    
    endRenderCommandAndPass(commandBuffer);
    

    VkSemaphore graphicsWaitSemaphores[] = { m_computerSemaphore, m_imageAvailableSemaphores[m_currentFrame] };
    VkSemaphore graphicsSignalSemaphores[] = { m_graphicsSemaphore, m_renderFinishedSemaphores[m_currentFrame] };

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 2;
    submitInfo.pWaitSemaphores = graphicsWaitSemaphores;
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 2;
    submitInfo.pSignalSemaphores = graphicsSignalSemaphores;

    //在命令缓冲区结束后需要发起的fence
    if(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to queue submit!");
    }
    
    queueResult();
    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchainKHR;
    presentInfo.pImageIndices = &m_imageIndex;
    presentInfo.pResults = nullptr;
    
    if(vkQueuePresentKHR(m_presentQueue, &presentInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to queue present!");
    }
    
    m_currentFrame = (m_currentFrame + 1) % m_swapchainImageCount;
}

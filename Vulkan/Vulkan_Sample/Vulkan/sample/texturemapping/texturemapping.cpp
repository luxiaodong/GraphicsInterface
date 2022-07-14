
#include "texturemapping.h"

TextureMapping::TextureMapping(std::string title) : Application(title)
{
}

TextureMapping::~TextureMapping()
{}

void TextureMapping::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void TextureMapping::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
    m_camera.setRotation(glm::vec3(0.0f, 15.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void TextureMapping::clear()
{
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_vertexMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexMemory, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    
    m_pTexture->clear();
    Application::clear();
}

void TextureMapping::prepareVertex()
{
    std::vector<Vertex> vertexs =
    {
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
    };

    std::vector<uint32_t> indexs = { 0,1,2, 2,3,0 };
    
    m_vertexInputBindDes.clear();
    m_vertexInputBindDes.push_back(Tools::getVertexInputBindingDescription(0, sizeof(Vertex)));
    
    m_vertexInputAttrDes.clear();
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)));
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)));
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)));

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
    
    m_pTexture = Texture::loadTextrue2D(Tools::getTexturePath() +  "metalplate01_rgba.ktx", m_graphicsQueue);
}

void TextureMapping::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.viewPos = m_camera.m_viewPos;
    mvp.lodBias = 0.0f;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void TextureMapping::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void TextureMapping::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);
    createDescriptorSet(m_descriptorSet);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    bufferInfo.buffer = m_uniformBuffer;
    
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_pTexture->m_imageView;
    imageInfo.sampler = m_pTexture->m_sampler;

    std::array<VkWriteDescriptorSet, 2> writes = {};
    writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
    writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void TextureMapping::createGraphicsPipeline()
{
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "texture/texture.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "texture/texture.frag.spv");
    
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
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

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

void TextureMapping::updateRenderData()
{
}

void TextureMapping::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
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
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
}


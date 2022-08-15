
#include "instancing.h"
#include <stdlib.h>
#include <random>

Instancing::Instancing(std::string title) : Application(title)
{
}

Instancing::~Instancing()
{}

void Instancing::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void Instancing::initCamera()
{
    m_camera.setTranslation(glm::vec3(5.5f, -1.85f, -18.5f));
    m_camera.setRotation(glm::vec3(-17.2f, -4.7f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void Instancing::setEnabledFeatures()
{
}

void Instancing::clear()
{
    vkDestroyPipeline(m_device, m_bgPipeline, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    
    vkDestroyPipeline(m_device, m_instanceRockPipeline, nullptr);
    vkDestroyBuffer(m_device, m_instanceBuffer, nullptr);
    vkFreeMemory(m_device, m_instanceMemory, nullptr);
    
    m_planetLoader.clear();
    m_rocksLoader.clear();
    m_pPlanet->clear();
    m_pRocks->clear();
    delete m_pPlanet;
    delete m_pRocks;
    Application::clear();
}

void Instancing::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    m_planetLoader.loadFromFile(Tools::getModelPath() + "lavaplanet.gltf", m_graphicsQueue, flags);
    m_planetLoader.createVertexAndIndexBuffer();
    m_planetLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV, VertexComponent::Color});
    m_rocksLoader.loadFromFile(Tools::getModelPath() + "rock01.gltf", m_graphicsQueue, flags);
    m_rocksLoader.createVertexAndIndexBuffer();
    
    m_pPlanet = Texture::loadTextrue2D(Tools::getTexturePath() + "lavaplanet_rgba.ktx", m_graphicsQueue);
    m_pRocks = Texture::loadTextrue2D(Tools::getTexturePath() + "texturearray_rocks_rgba.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion::Layer);
}

void Instancing::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        m_uniformBuffer, m_uniformMemory);
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.lightPos = glm::vec4(0.0f, -5.0f, 0.0f, 1.0f);
    mvp.locSpeed = 0.35f;
    mvp.globSpeed = 0.01f;

    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
    
    prepareInstanceData();
}

void Instancing::prepareInstanceData()
{
    std::vector<InstanceData> instanceData;
    instanceData.resize(INSTANCE_COUNT);
    
    std::default_random_engine rndGenerator((unsigned)time(nullptr));
    std::uniform_real_distribution<float> uniformDist(0.0, 1.0);
    std::uniform_int_distribution<uint32_t> rndTextureIndex(0, m_pRocks->m_layerCount);
    
    // Distribute rocks randomly on two different rings
    for (auto i = 0; i < INSTANCE_COUNT / 2; i++)
    {
        glm::vec2 ring0 { 7.0f, 11.0f };
        glm::vec2 ring1 { 14.0f, 18.0f };

        float rho, theta;

        // Inner ring
        rho = sqrt((pow(ring0[1], 2.0f) - pow(ring0[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring0[0], 2.0f));
        theta = 2.0 * M_PI * uniformDist(rndGenerator);
        instanceData[i].pos = glm::vec3(rho*cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho*sin(theta));
        instanceData[i].rot = glm::vec3(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator));
        instanceData[i].scale = 1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator);
        instanceData[i].texIndex = rndTextureIndex(rndGenerator);
        instanceData[i].scale *= 0.75f;

        // Outer ring
        rho = sqrt((pow(ring1[1], 2.0f) - pow(ring1[0], 2.0f)) * uniformDist(rndGenerator) + pow(ring1[0], 2.0f));
        theta = 2.0 * M_PI * uniformDist(rndGenerator);
        instanceData[i + INSTANCE_COUNT / 2].pos = glm::vec3(rho*cos(theta), uniformDist(rndGenerator) * 0.5f - 0.25f, rho*sin(theta));
        instanceData[i + INSTANCE_COUNT / 2].rot = glm::vec3(M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator), M_PI * uniformDist(rndGenerator));
        instanceData[i + INSTANCE_COUNT / 2].scale = 1.5f + uniformDist(rndGenerator) - uniformDist(rndGenerator);
        instanceData[i + INSTANCE_COUNT / 2].texIndex = rndTextureIndex(rndGenerator);
        instanceData[i + INSTANCE_COUNT / 2].scale *= 0.75f;
    }
    
    VkDeviceSize instanceSize = sizeof(InstanceData) * INSTANCE_COUNT;
    Tools::createBufferAndMemoryThenBind(instanceSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        m_instanceBuffer, m_instanceMemory);
    Tools::mapMemory(m_instanceMemory, instanceSize, instanceData.data());
}

void Instancing::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void Instancing::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);

    {
        createDescriptorSet(m_descriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pPlanet->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_instanceRockdescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pRocks->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_instanceRockdescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_instanceRockdescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void Instancing::createGraphicsPipeline()
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
    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    rasterization.cullMode = VK_CULL_MODE_NONE;
    depthStencil.depthWriteEnable = VK_FALSE;
    VkPipelineVertexInputStateCreateInfo emptyInputState = {};
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.pVertexInputState = &emptyInputState;
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "instancing/starfield.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "instancing/starfield.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_bgPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    depthStencil.depthWriteEnable = VK_TRUE;
    createInfo.pVertexInputState = m_planetLoader.getPipelineVertexInputState();
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "instancing/planet.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "instancing/planet.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // instancing
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    std::array<VkVertexInputBindingDescription, 2> bindingDescriptions = {};
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    bindingDescriptions[1].binding = 1;
    bindingDescriptions[1].stride  = sizeof(InstanceData);
    
    std::array<VkVertexInputAttributeDescription, 8> attributeDescriptions = {};
    attributeDescriptions[0] = Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, m_position));
    attributeDescriptions[1] = Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, m_normal));
    attributeDescriptions[2] = Tools::getVertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, m_uv));
    attributeDescriptions[3] = Tools::getVertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, m_color));
    attributeDescriptions[4] = Tools::getVertexInputAttributeDescription(1, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, pos));
    attributeDescriptions[5] = Tools::getVertexInputAttributeDescription(1, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, rot));
    attributeDescriptions[6] = Tools::getVertexInputAttributeDescription(1, 6, VK_FORMAT_R32_SFLOAT, offsetof(InstanceData, scale));
    attributeDescriptions[7] = Tools::getVertexInputAttributeDescription(1, 7, VK_FORMAT_R32_SINT, offsetof(InstanceData, texIndex));
    
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInput.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();
    createInfo.pVertexInputState = &vertexInput;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "instancing/instancing.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "instancing/instancing.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_instanceRockPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void Instancing::updateRenderData()
{
}

void Instancing::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bgPipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_planetLoader.bindBuffers(commandBuffer);
    m_planetLoader.draw(commandBuffer);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_instanceRockPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_instanceRockdescriptorSet, 0, nullptr);

    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_rocksLoader.m_vertexBuffer, offsets);
    vkCmdBindVertexBuffers(commandBuffer, 1, 1, &m_instanceBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_rocksLoader.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_rocksLoader.m_indexData.size()), INSTANCE_COUNT, 0, 0, 0);
}


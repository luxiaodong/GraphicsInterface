
#include "indirectdraw.h"
#include <stdlib.h>
#include <random>

#define OBJECT_INSTANCE_COUNT 1024

IndirectDraw::IndirectDraw(std::string title) : Application(title)
{
}

IndirectDraw::~IndirectDraw()
{}

void IndirectDraw::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void IndirectDraw::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.setTranslation(glm::vec3(0.4f, 1.25f, 0.0f));
    m_camera.setRotation(glm::vec3(-12.0f, 159.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void IndirectDraw::setEnabledFeatures()
{
    if(m_deviceFeatures.multiDrawIndirect)
    {
        m_deviceEnabledFeatures.multiDrawIndirect = true;
    }
}

void IndirectDraw::clear()
{
    vkDestroyPipeline(m_device, m_skyPipeline, nullptr);
    
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    
    vkDestroyPipeline(m_device, m_plantsPipeline, nullptr);
    vkDestroyBuffer(m_device, m_instanceBuffer, nullptr);
    vkFreeMemory(m_device, m_instanceMemory, nullptr);
    
    m_skysphereLoader.clear();
    m_groundLoader.clear();
    m_plantsLoader.clear();
    m_pPlants->clear();
    m_pGround->clear();
    delete m_pPlants;
    delete m_pGround;
    Application::clear();
}

void IndirectDraw::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    m_skysphereLoader.loadFromFile(Tools::getModelPath() + "sphere.gltf", m_graphicsQueue, flags);
    m_skysphereLoader.createVertexAndIndexBuffer();
    m_skysphereLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV, VertexComponent::Color});
    m_groundLoader.loadFromFile(Tools::getModelPath() + "plane_circle.gltf", m_graphicsQueue, flags);
    m_groundLoader.createVertexAndIndexBuffer();
    m_plantsLoader.loadFromFile(Tools::getModelPath() + "plants.gltf", m_graphicsQueue, flags);
    m_plantsLoader.createVertexAndIndexBuffer();
    
    m_pGround = Texture::loadTextrue2D(Tools::getTexturePath() + "ground_dry_rgba.ktx", m_graphicsQueue);
    m_pPlants = Texture::loadTextrue2D(Tools::getTexturePath() + "texturearray_plants_rgba.ktx", m_graphicsQueue, VK_FORMAT_R8G8B8A8_UNORM, TextureCopyRegion::Layer);
}

void IndirectDraw::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
    
    prepareIndirectData();
    prepareInstanceData();
}

void IndirectDraw::prepareIndirectData()
{
    m_indirectCommands.clear();
    
    // Create on indirect command for node in the scene with a mesh attached to it
    uint32_t m = 0;
    for (auto &node : m_plantsLoader.m_linearNodes)
    {
        if (node->m_mesh)
        {
            VkDrawIndexedIndirectCommand indirectCmd = {};
            indirectCmd.instanceCount = OBJECT_INSTANCE_COUNT;
            indirectCmd.firstInstance = m * OBJECT_INSTANCE_COUNT;
            // @todo: Multiple primitives
            // A glTF node may consist of multiple primitives, so we may have to do multiple commands per mesh
            indirectCmd.firstIndex = node->m_mesh->m_primitives[0]->m_indexOffset;
            indirectCmd.indexCount = node->m_mesh->m_primitives[0]->m_indexCount;

            m_indirectCommands.push_back(indirectCmd);
            m++;
        }
    }
    
    m_objectCount = 0;
    for (auto indirectCmd : m_indirectCommands)
    {
        m_objectCount += indirectCmd.instanceCount;
    }
    
    VkDeviceSize instanceSize = sizeof(VkDrawIndexedIndirectCommand) * m_indirectCommands.size();
    Tools::createBufferAndMemoryThenBind(instanceSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_indirectBuffer, m_indirectMemory);
    
    Tools::mapMemory(m_indirectMemory, instanceSize, m_indirectCommands.data());
}

void IndirectDraw::prepareInstanceData()
{
    std::vector<InstanceData> instanceData;
    instanceData.resize(m_objectCount);

    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);

    for (uint32_t i = 0; i < m_objectCount; i++)
    {
        float theta = 2 * float(M_PI) * uniformDist(rndEngine);
        float phi = acos(1 - 2 * uniformDist(rndEngine));
        instanceData[i].rot = glm::vec3(0.0f, float(M_PI) * uniformDist(rndEngine), 0.0f);
        instanceData[i].pos = glm::vec3(sin(phi) * cos(theta), 0.0f, cos(phi)) * 25.0f;
        instanceData[i].scale = 1.0f + uniformDist(rndEngine) * 2.0f;
        instanceData[i].texIndex = i / OBJECT_INSTANCE_COUNT;
    }
    
    VkDeviceSize instanceSize = sizeof(InstanceData) * instanceData.size();
    Tools::createBufferAndMemoryThenBind(instanceSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        m_instanceBuffer, m_instanceMemory);
    Tools::mapMemory(m_instanceMemory, instanceSize, instanceData.data());
}

void IndirectDraw::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void IndirectDraw::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);

    {
        createDescriptorSet(m_descriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo1 = m_pPlants->getDescriptorImageInfo();
        VkDescriptorImageInfo imageInfo2 = m_pGround->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo1);
        writes[2] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void IndirectDraw::createGraphicsPipeline()
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
    
    // sky
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    createInfo.pVertexInputState = m_skysphereLoader.getPipelineVertexInputState();
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "indirectdraw/skysphere.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "indirectdraw/skysphere.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_skyPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // ground
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    createInfo.pVertexInputState = m_groundLoader.getPipelineVertexInputState();
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "indirectdraw/ground.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "indirectdraw/ground.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // plant
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
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "indirectdraw/indirectdraw.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "indirectdraw/indirectdraw.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_plantsPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void IndirectDraw::updateRenderData()
{
}

void IndirectDraw::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyPipeline);
    m_skysphereLoader.bindBuffers(commandBuffer);
    m_skysphereLoader.draw(commandBuffer);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    m_groundLoader.bindBuffers(commandBuffer);
    m_groundLoader.draw(commandBuffer);
    
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_instanceRockPipeline);
//    const VkDeviceSize offsets[1] = {0};
//    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_rocksLoader.m_vertexBuffer, offsets);
//    vkCmdBindVertexBuffers(commandBuffer, 1, 1, &m_instanceBuffer, offsets);
//    vkCmdBindIndexBuffer(commandBuffer, m_rocksLoader.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
//    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_rocksLoader.m_indexData.size()), INSTANCE_COUNT, 0, 0, 0);
}


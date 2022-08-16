
#include "pipelinestatistics.h"
#include <stdlib.h>
#include <random>

PipelineStatistics::PipelineStatistics(std::string title) : Application(title)
{
}

PipelineStatistics::~PipelineStatistics()
{}

void PipelineStatistics::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void PipelineStatistics::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.setPosition(glm::vec3(-3.0f, 1.0f, -2.75f));
    m_camera.setRotation(glm::vec3(-15.25f, -46.5f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void PipelineStatistics::setEnabledFeatures()
{
    if(m_deviceFeatures.pipelineStatisticsQuery)
    {
        m_deviceEnabledFeatures.pipelineStatisticsQuery = VK_TRUE;
    }
    else
    {
        throw std::runtime_error("Selected GPU does not support pipeline statistics!");
    }
    
    if(m_deviceFeatures.fillModeNonSolid)
    {
        m_deviceEnabledFeatures.fillModeNonSolid = VK_TRUE;
    }
    
    if(m_deviceFeatures.tessellationShader)
    {
        m_deviceEnabledFeatures.tessellationShader = VK_TRUE;
    }
}

void PipelineStatistics::clear()
{
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    
    vkDestroyQueryPool(m_device, m_queryPool, nullptr);
    Application::clear();
}

void PipelineStatistics::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::FlipY;
    std::vector<std::string> fileNames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
    m_gltfLoader.loadFromFile(Tools::getModelPath() + fileNames[3], m_graphicsQueue, flags);
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
    
    prepareQueryPool();
}

void PipelineStatistics::prepareQueryPool()
{
    m_pipelineStatNames.clear();
    m_pipelineStatValues.clear();
    
    m_pipelineStatNames.push_back("Input assembly vertex count        ");
    m_pipelineStatNames.push_back("Input assembly primitives count    ");
    m_pipelineStatNames.push_back("Vertex shader invocations          ");
    m_pipelineStatNames.push_back("Clipping stage primitives processed");
    m_pipelineStatNames.push_back("Clipping stage primitives output   ");
    m_pipelineStatNames.push_back("Fragment shader invocations        ");
    
    if(m_deviceEnabledFeatures.tessellationShader)
    {
        m_pipelineStatNames.push_back("Tess. control shader patches       ");
        m_pipelineStatNames.push_back("Tess. eval. shader invocations     ");
    }
    
    m_pipelineStatValues.resize(m_pipelineStatNames.size());
    
    VkQueryPoolCreateInfo queryPoolInfo = {};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    queryPoolInfo.pipelineStatistics =
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
        VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;
            
    if(m_deviceEnabledFeatures.tessellationShader)
    {
        queryPoolInfo.pipelineStatistics |=
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
            VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT;
    }

    queryPoolInfo.queryCount = static_cast<uint32_t>(m_pipelineStatNames.size());
    VK_CHECK_RESULT(vkCreateQueryPool(m_device, &queryPoolInfo, NULL, &m_queryPool));
}

void PipelineStatistics::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
}

void PipelineStatistics::prepareDescriptorSetLayoutAndPipelineLayout()
{
    VkPushConstantRange constantRange = {};
    constantRange.offset = 0;
    constantRange.size = sizeof(glm::vec3);
    constantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    std::array<VkDescriptorSetLayoutBinding, 1> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout(&constantRange, 1);
}

void PipelineStatistics::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 1> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);
    
    {
        createDescriptorSet(m_descriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void PipelineStatistics::createGraphicsPipeline()
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
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(m_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(m_blending ? VK_TRUE : VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    rasterization.rasterizerDiscardEnable = m_discard ? VK_TRUE : VK_FALSE;
    
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.resize(m_tessellation ? 4 : 2);
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
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "pipelinestatistics/scene.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "pipelinestatistics/scene.frag.spv");
    VkShaderModule tescModule = VK_NULL_HANDLE;
    VkShaderModule teseModule = VK_NULL_HANDLE;
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    
    if(m_tessellation == true)
    {
        tescModule = Tools::createShaderModule( Tools::getShaderPath() + "pipelinestatistics/scene.tesc.spv");
        teseModule = Tools::createShaderModule( Tools::getShaderPath() + "pipelinestatistics/scene.tese.spv");
        shaderStages[2] = Tools::getPipelineShaderStageCreateInfo(tescModule, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        shaderStages[3] = Tools::getPipelineShaderStageCreateInfo(teseModule, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        
        VkPipelineTessellationStateCreateInfo tessellation = Tools::getPipelineTessellationStateCreateInfo(3);
        createInfo.pTessellationState = &tessellation;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }
    
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    if(m_tessellation == true)
    {
        vkDestroyShaderModule(m_device, tescModule, nullptr);
        vkDestroyShaderModule(m_device, teseModule, nullptr);
    }
}

void PipelineStatistics::updateRenderData()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
}

void PipelineStatistics::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_gltfLoader.bindBuffers(commandBuffer);
    
    vkCmdBeginQuery(commandBuffer, m_queryPool, 0, 0);
    for (int32_t y = 0; y < 3; y++) {
        for (int32_t x = 0; x < 3; x++) {
            glm::vec3 pos = glm::vec3(float(x - (3 / 2.0f)) * 2.5f, 0.0f, float(y - (3 / 2.0f)) * 2.5f);
            vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
            m_gltfLoader.draw(commandBuffer);
        }
    }
    vkCmdEndQuery(commandBuffer, m_queryPool, 0);
}

void PipelineStatistics::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    vkCmdResetQueryPool(commandBuffer, m_queryPool, 0, static_cast<uint32_t>(m_pipelineStatNames.size()));
}

void PipelineStatistics::queueResult()
{
    vkGetQueryPoolResults(m_device, m_queryPool, 0, 1, m_pipelineStatValues.size()*sizeof(uint64_t), m_pipelineStatValues.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
  
    for(auto  i = 0; i < m_pipelineStatNames.size(); ++i)
    {
        std::cout << m_pipelineStatNames[i] << m_pipelineStatValues[i] << std::endl;
    }
}



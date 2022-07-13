
#include "dynamicuniformbuffer.h"
#include <stdlib.h>
#include <ctime>
#include <random>
#include <malloc/_malloc.h>

DynamicUniformBuffer::DynamicUniformBuffer(std::string title) : Application(title)
{
}

DynamicUniformBuffer::~DynamicUniformBuffer()
{}

void DynamicUniformBuffer::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void DynamicUniformBuffer::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -12.5f));
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void DynamicUniformBuffer::clear()
{
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_modelMemory, nullptr);
    vkDestroyBuffer(m_device, m_modelBuffer, nullptr);
    
    vkFreeMemory(m_device, m_vertexMemory, nullptr);
    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_device, m_indexMemory, nullptr);
    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    
    Application::clear();
}

void DynamicUniformBuffer::prepareVertex()
{
    std::vector<Vertex> vertices = {
        { { -1.0f, -1.0f,  1.0f },{ 1.0f, 0.0f, 0.0f } },
        { {  1.0f, -1.0f,  1.0f },{ 0.0f, 1.0f, 0.0f } },
        { {  1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f } },
        { { -1.0f, -1.0f, -1.0f },{ 1.0f, 0.0f, 0.0f } },
        { {  1.0f, -1.0f, -1.0f },{ 0.0f, 1.0f, 0.0f } },
        { {  1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 1.0f } },
        { { -1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f } },
    };

    std::vector<uint32_t> indices = {
        0,1,2, 2,3,0, 1,5,6, 6,2,1, 7,6,5, 5,4,7, 4,0,3, 3,7,4, 4,5,1, 1,0,4, 3,2,6, 6,7,3,
    };
    
    VkDeviceSize vertexSize = vertices.size() * sizeof(Vertex);
    VkDeviceSize indexSize = indices.size() * sizeof(uint32_t);
    
    Tools::createBufferAndMemoryThenBind(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_vertexBuffer, m_vertexMemory);
    Tools::mapMemory(m_vertexMemory, vertexSize, vertices.data());
    
    Tools::createBufferAndMemoryThenBind(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_indexBuffer, m_indexMemory);
    Tools::mapMemory(m_vertexMemory, indexSize, indices.data());
    
    
    m_vertexInputBindDes.clear();
    m_vertexInputBindDes.push_back(Tools::getVertexInputBindingDescription(0, sizeof(Vertex)));
    
    m_vertexInputAttrDes.clear();
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)));
    m_vertexInputAttrDes.push_back(Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)));
    
    m_indexCount = static_cast<uint32_t>(indices.size());
}

void DynamicUniformBuffer::dynamicAlignment()
{
    m_matrixAlignment = sizeof(glm::mat4);
    size_t minUniformAlignment = m_deviceProperties.limits.minUniformBufferOffsetAlignment;
    if(minUniformAlignment >  0)
    {
        m_matrixAlignment = (m_matrixAlignment + minUniformAlignment - 1) & ~(minUniformAlignment - 1);
    }
}

void DynamicUniformBuffer::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(DynamicUniformBuffer::Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    DynamicUniformBuffer::Uniform vp = {};
    vp.viewMatrix = m_camera.m_viewMat;
    vp.projectionMatrix = m_camera.m_projMat;
    Tools::mapMemory(m_uniformMemory, sizeof(DynamicUniformBuffer::Uniform), &vp);
    
    dynamicAlignment();
    size_t totalSize = OBJECT_INSTANCES * m_matrixAlignment;
    
    DynamicUniformBuffer::UniformDynamic m = {};
    m.pModelMatrix = (unsigned char*)aligned_alloc(m_matrixAlignment, totalSize);
    
    glm::mat4 idMat(1.0);
    for (uint32_t i = 0; i < OBJECT_INSTANCES; i++)
    {
        unsigned char* p  =  m.pModelMatrix + (i*m_matrixAlignment);
        memcpy(p, &idMat, m_matrixAlignment);
    }
    
    Tools::createBufferAndMemoryThenBind(totalSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_modelBuffer, m_modelMemory);

    Tools::mapMemory(m_modelMemory, totalSize, m.pModelMatrix);
    free(m.pModelMatrix);
//    m_m = m;
}

void DynamicUniformBuffer::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, 1);
    
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void DynamicUniformBuffer::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[1].descriptorCount = 1;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 1);
    createDescriptorSet(m_descriptorSet);
    
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    bufferInfo.buffer = m_uniformBuffer;
    
    VkDescriptorBufferInfo bufferInfo2 = {};
    bufferInfo2.offset = 0;
    bufferInfo2.range = m_matrixAlignment;
    bufferInfo2.buffer = m_modelBuffer;
    
    std::array<VkWriteDescriptorSet, 2> writes = {};
    writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
    writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &bufferInfo2);
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void DynamicUniformBuffer::createGraphicsPipeline()
{
    VkShaderModule vertModule = Tools::createShaderModule(Tools::getShaderPath() + "dynamicuniformbuffer/base.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule(Tools::getShaderPath() + "dynamicuniformbuffer/base.frag.spv");
    
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

void DynamicUniformBuffer::prepareRenderData()
{
}

void DynamicUniformBuffer::recordRenderCommand(const VkCommandBuffer commandBuffer)
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
    
    for (uint32_t i = 0; i < OBJECT_INSTANCES; i++)
    {
        uint32_t dynamicOffset = i * static_cast<uint32_t>(m_matrixAlignment);
        
//        unsigned char* p  = m_m.pModelMatrix + (i*m_matrixAlignment);
//        glm::mat4* pMat = (glm::mat4*)p;
        
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 1, &dynamicOffset);
        vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
    }
}

//    std::default_random_engine rndEngine((unsigned)time(nullptr));
//    std::normal_distribution<float> rndDist(-1.0f, 1.0f);
//     Dynamic ubo with per-object model matrices indexed by offsets in the command buffer
//    uint32_t dim = static_cast<uint32_t>(pow(OBJECT_INSTANCES, (1.0f / 3.0f)));
//    glm::vec3 offset(5.0f);
//
//    for (uint32_t x = 0; x < dim; x++)
//    {
//        for (uint32_t y = 0; y < dim; y++)
//        {
//            for (uint32_t z = 0; z < dim; z++)
//            {
//                uint32_t index = x * dim * dim + y * dim + z;
//                // Aligned offset
//                glm::mat4* modelMat = (glm::mat4*)(((uint64_t)m_pModelMatrix + (index * m_matrixAlignment)));
//
//                // Update rotations
//                glm::vec3 rotations = glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine)) * 2.0f * (float)M_PI;
//
//                // Update matrices
//                glm::vec3 pos = glm::vec3(-((dim * offset.x) / 2.0f) + offset.x / 2.0f + x * offset.x, -((dim * offset.y) / 2.0f) + offset.y / 2.0f + y * offset.y, -((dim * offset.z) / 2.0f) + offset.z / 2.0f + z * offset.z);
//                *modelMat = glm::translate(glm::mat4(1.0f), pos);
//                *modelMat = glm::rotate(*modelMat, rotations.x, glm::vec3(1.0f, 1.0f, 0.0f));
//                *modelMat = glm::rotate(*modelMat, rotations.y, glm::vec3(0.0f, 1.0f, 0.0f));
//                *modelMat = glm::rotate(*modelMat, rotations.z, glm::vec3(0.0f, 0.0f, 1.0f));
//            }
//        }
//    }

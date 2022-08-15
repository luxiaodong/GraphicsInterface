
#include "multithread.h"

MultiThread::MultiThread(std::string title) : Application(title)
{
    m_subpassContents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
}

MultiThread::~MultiThread()
{}

void MultiThread::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    prepareMultiThread();
    createSecondaryCommandBuffer();

    createGraphicsPipeline();
}

void MultiThread::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f,-32.5f));
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(60.0f, (float)m_width/(float)m_height, 0.1f, 256.0f);
    
    m_threadCount = std::thread::hardware_concurrency();
    assert(m_threadCount > 0);
    std::cout << "thread count = " << m_threadCount << std::endl;
    m_threadPool.setThreadCount(m_threadCount);
    m_objectCountPerThread = 512 / m_threadCount;
}

void MultiThread::setEnabledFeatures()
{
}

void MultiThread::clear()
{
//    vkFreeMemory(m_device, m_uniformMemory, nullptr);
//    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);

    for (uint32_t i = 0; i < m_threadCount; i++)
    {
        ThreadData *threadData = m_threadDatas[i];
        vkFreeCommandBuffers(m_device, threadData->commandPool, static_cast<uint32_t>(threadData->commandBuffer.size()), threadData->commandBuffer.data());
        vkDestroyCommandPool(m_device, threadData->commandPool, nullptr);
        delete threadData;
    }
    
    vkDestroyPipeline(m_device, m_ufoPipeline, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);

    m_ufoLoader.clear();
    m_sphereLoader.clear();
    Application::clear();
}

void MultiThread::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    
    m_ufoLoader.loadFromFile(Tools::getModelPath() + "retroufo_red_lowpoly.gltf", m_graphicsQueue, flags);
    m_ufoLoader.createVertexAndIndexBuffer();
    m_ufoLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
    
    m_sphereLoader.loadFromFile(Tools::getModelPath() + "sphere.gltf", m_graphicsQueue, flags);
    m_sphereLoader.createVertexAndIndexBuffer();
    m_sphereLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
}

void MultiThread::prepareUniform()
{
}

void MultiThread::prepareDescriptorSetLayoutAndPipelineLayout()
{
    m_ufoPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    m_ufoPushConstantRange.offset = 0;
    m_ufoPushConstantRange.size = sizeof(PushConstantBlock);
    
    //ufo && sphere
    {
        createPipelineLayout(nullptr, 0, m_pipelineLayout, &m_ufoPushConstantRange, 1);
    }
}

void MultiThread::prepareDescriptorSetAndWrite()
{
    // 没有 unifom && image
}

void MultiThread::createGraphicsPipeline()
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
    
//    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;

    //ufo
    createInfo.pVertexInputState = m_ufoLoader.getPipelineVertexInputState();
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "multithreading/phong.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "multithreading/phong.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_ufoPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
 
    //star
    createInfo.pVertexInputState = m_sphereLoader.getPipelineVertexInputState();
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    depthStencil.depthWriteEnable = VK_FALSE;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "multithreading/starsphere.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "multithreading/starsphere.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void MultiThread::updateRenderData()
{
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    m_frustum.update(mvp.projectionMatrix * mvp.viewMatrix);
}

void MultiThread::recordRenderCommand(const VkCommandBuffer primaryCommandBuffer)
{
    std::vector<VkCommandBuffer> commandBuffers;
    
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.renderPass = m_renderPass;
    inheritanceInfo.framebuffer = m_framebuffers[m_imageIndex];
    
    updateSecondaryCommandBuffers(inheritanceInfo);
    
    commandBuffers.push_back(m_secondaryCommandBuffer);
    
    // Add a job to the thread's queue for each object to be rendered
    for (uint32_t t = 0; t < m_threadCount; t++)
    {
        for (uint32_t i = 0; i < m_objectCountPerThread; i++)
        {
            m_threadPool.m_threads[t]->addJob([=] { threadRenderCode(t, i, inheritanceInfo); });
        }
    }

    m_threadPool.wait();

    // Only submit if object is within the current view frustum
    for (uint32_t t = 0; t < m_threadCount; t++)
    {
        for (uint32_t i = 0; i < m_objectCountPerThread; i++)
        {
            if (m_threadDatas[t]->objectData[i].visible)
            {
                commandBuffers.push_back(m_threadDatas[t]->commandBuffer[i]);
            }
        }
    }
 
    vkCmdExecuteCommands(primaryCommandBuffer, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
}

void MultiThread::createSecondaryCommandBuffer()
{
    VkCommandBufferAllocateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    createInfo.commandPool = m_commandPool;
    createInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    createInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_device, &createInfo, &m_secondaryCommandBuffer);
}

void MultiThread::prepareMultiThread()
{
    m_threadDatas.clear();
    
    for (uint32_t i = 0; i < m_threadCount; i++)
    {
        ThreadData *threadData = new ThreadData();
        m_threadDatas.push_back(threadData);
        
        {
            VkCommandPoolCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            createInfo.queueFamilyIndex = m_familyIndices.graphicsFamily.value();
            vkCreateCommandPool(m_device, &createInfo, nullptr, &threadData->commandPool);
        }
        
        // One secondary command buffer per object that is updated by this thread
        threadData->commandBuffer.resize(m_objectCountPerThread);
        threadData->pushConstBlock.resize(m_objectCountPerThread);
        threadData->objectData.resize(m_objectCountPerThread);
        
        {
            // Generate secondary command buffers for each thread
            VkCommandBufferAllocateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            createInfo.commandPool = threadData->commandPool;
            createInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
            createInfo.commandBufferCount = static_cast<uint32_t>(threadData->commandBuffer.size());
            vkAllocateCommandBuffers(m_device, &createInfo, threadData->commandBuffer.data());
        }
       
        for(uint32_t j = 0; j < m_objectCountPerThread; ++j)
        {
            float theta = 2.0f * float(M_PI) * Tools::random01();
            float phi = acos(1.0f - 2.0f * Tools::random01());
            threadData->objectData[j].pos = glm::vec3(sin(phi) * cos(theta), 0.0f, cos(phi)) * 35.0f;

            threadData->objectData[j].rotation = glm::vec3(0.0f, Tools::random01()*360.0f, 0.0f);
            threadData->objectData[j].deltaT = Tools::random01();
            threadData->objectData[j].rotationDir = (Tools::random01()*100.0f < 50.0f) ? 1.0f : -1.0f;
            threadData->objectData[j].rotationSpeed = (2.0f + Tools::random01()*4.0f) * threadData->objectData[j].rotationDir;
            threadData->objectData[j].scale = 0.75f + Tools::random01()*0.5f;
            threadData->pushConstBlock[j].color = glm::vec3(Tools::random01(), Tools::random01(), Tools::random01());
        }
    }
}

void MultiThread::updateSecondaryCommandBuffers(VkCommandBufferInheritanceInfo inheritanceInfo)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    beginInfo.pInheritanceInfo = &inheritanceInfo;
    
    vkBeginCommandBuffer(m_secondaryCommandBuffer, &beginInfo);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetViewport(m_secondaryCommandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(m_secondaryCommandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(m_secondaryCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    
    glm::mat4 mvp = m_camera.m_projMat * m_camera.m_viewMat;
    mvp[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    mvp = glm::scale(mvp, glm::vec3(2.0f));

    vkCmdPushConstants(m_secondaryCommandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), &mvp);
    m_sphereLoader.bindBuffers(m_secondaryCommandBuffer);
    m_sphereLoader.draw(m_secondaryCommandBuffer);
    
    VK_CHECK_RESULT(vkEndCommandBuffer(m_secondaryCommandBuffer));
}

void MultiThread::threadRenderCode(uint32_t threadIndex, uint32_t cmdBufferIndex, VkCommandBufferInheritanceInfo inheritanceInfo)
{
    ThreadData* threadData = m_threadDatas[threadIndex];
    ObjectData* objectData = &threadData->objectData[cmdBufferIndex];

    // Check visibility against view frustum using a simple sphere check based on the radius of the mesh
    objectData->visible = m_frustum.checkSphere(objectData->pos,  m_ufoLoader.m_radius * 0.5f); // models.ufo.dimensions.radius
    // objectData->visible = false;
    if(objectData->visible == false) return ;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    beginInfo.pInheritanceInfo = &inheritanceInfo;

    VkCommandBuffer cmdBuffer = threadData->commandBuffer[cmdBufferIndex];
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
    
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    
    static float frameTimer = 0.005f;
    objectData->rotation.y += 2.5f * objectData->rotationSpeed * frameTimer;
    if (objectData->rotation.y > 360.0f) {
        objectData->rotation.y -= 360.0f;
    }
    objectData->deltaT += 0.15f * frameTimer;
    if (objectData->deltaT > 1.0f)
        objectData->deltaT -= 1.0f;
    objectData->pos.y = sin(glm::radians(objectData->deltaT * 360.0f)) * 2.5f;
//    frameTimer += 0.0001f;

    objectData->model = glm::translate(glm::mat4(1.0f), objectData->pos);
    objectData->model = glm::rotate(objectData->model, -sinf(glm::radians(objectData->deltaT * 360.0f)) * 0.25f, glm::vec3(objectData->rotationDir, 0.0f, 0.0f));
    objectData->model = glm::rotate(objectData->model, glm::radians(objectData->rotation.y), glm::vec3(0.0f, objectData->rotationDir, 0.0f));
    objectData->model = glm::rotate(objectData->model, glm::radians(objectData->deltaT * 360.0f), glm::vec3(0.0f, objectData->rotationDir, 0.0f));
    objectData->model = glm::scale(objectData->model, glm::vec3(objectData->scale));

    threadData->pushConstBlock[cmdBufferIndex].mvp = m_camera.m_projMat * m_camera.m_viewMat * objectData->model;

    vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantBlock), &threadData->pushConstBlock[cmdBufferIndex]);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ufoPipeline);
    m_ufoLoader.bindBuffers(cmdBuffer);
    m_ufoLoader.draw(cmdBuffer);
    VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));
}

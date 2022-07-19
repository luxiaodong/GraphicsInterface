
#include "subpasses.h"
#include <ctime>
#include <random>

SubPasses::SubPasses(std::string title) : Application(title)
{
}

SubPasses::~SubPasses()
{}

void SubPasses::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void SubPasses::initCamera()
{
    m_camera.setPosition(glm::vec3(-3.2f, 1.0f, 5.9f));
    m_camera.setRotation(glm::vec3(0.5f, 210.05f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void SubPasses::clear()
{
    vkDestroyPipeline(m_device, m_scenePipeline, nullptr);
    vkDestroyPipeline(m_device, m_compositePipeline, nullptr);
    vkDestroyPipeline(m_device, m_transparentPipeline, nullptr);
    
    vkDestroyPipelineLayout(m_device, m_scenePipelineLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_compositePipelineLayout, nullptr);
    
    vkDestroyDescriptorSetLayout(m_device, m_sceneDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_compositeDescriptorSetLayout, nullptr);
    
    vkFreeMemory(m_device, m_sceneUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_sceneUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_lightUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_lightUniformBuffer, nullptr);

    vkDestroyImageView(m_device, m_positionImageView, nullptr);
    vkDestroyImage(m_device, m_positionImage, nullptr);
    vkFreeMemory(m_device, m_positionMemory, nullptr);
    vkDestroyImageView(m_device, m_normalImageView, nullptr);
    vkDestroyImage(m_device, m_normalImage, nullptr);
    vkFreeMemory(m_device, m_normalMemory, nullptr);
    vkDestroyImageView(m_device, m_albedoImageView, nullptr);
    vkDestroyImage(m_device, m_albedoImage, nullptr);
    vkFreeMemory(m_device, m_albedoMemory, nullptr);

    m_pGlassTexture->clear();
    delete m_pGlassTexture;
    m_sceneLoader.clear();
    m_transparentLoader.clear();
    Application::clear();
}

void SubPasses::prepareVertex()
{
    m_sceneLoader.loadFromFile(Tools::getModelPath() + "samplebuilding.gltf", m_graphicsQueue);
    m_sceneLoader.createVertexAndIndexBuffer();
    m_sceneLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Color, VertexComponent::Normal, VertexComponent::UV});
    
    m_transparentLoader.loadFromFile(Tools::getModelPath() + "samplebuilding_glass.gltf", m_graphicsQueue);
    m_transparentLoader.createVertexAndIndexBuffer();
    m_pGlassTexture = Texture::loadTextrue2D(Tools::getTexturePath() + "colored_glass_rgba.ktx", m_graphicsQueue);
}

void SubPasses::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_sceneUniformBuffer, m_sceneUniformMemory);

    Uniform mvp = {};
    mvp.modelMatrix = glm::mat4(1.0f);
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.projectionMatrix = m_camera.m_projMat;
    Tools::mapMemory(m_sceneUniformMemory, uniformSize, &mvp);
    
    
    VkDeviceSize lightsSize = sizeof(ViewPosAndLights);
    Tools::createBufferAndMemoryThenBind(lightsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_lightUniformBuffer, m_lightUniformMemory);
    
    ViewPosAndLights lights = {};
    lights.viewPos = m_camera.m_viewPos;
    
    std::vector<glm::vec3> colors =
    {
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
    };
    
    std::default_random_engine rndGen((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);
    std::uniform_int_distribution<uint32_t> rndCol(0, static_cast<uint32_t>(colors.size()-1));
    
    for(int i = 0; i < NUM_LIGHTS; ++i)
    {
        lights.lights[i].position = glm::vec4(rndDist(rndGen) * 6.0f, 0.25f + std::abs(rndDist(rndGen)) * 4.0f, rndDist(rndGen) * 6.0f, 1.0f);
        lights.lights[i].color = colors[rndCol(rndGen)];
        lights.lights[i].radius = 1.0f + std::abs(rndDist(rndGen));
    }
    
    Tools::mapMemory(m_lightUniformMemory, lightsSize, &lights);
}

void SubPasses::createOtherBuffer()
{
    int width = m_swapchainExtent.width;
    int height = m_swapchainExtent.height;
    
    m_positionFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    Tools::createImageAndMemoryThenBind(m_positionFormat, width, height, 1, 1,
                                 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                 VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 m_positionImage, m_positionMemory);
    Tools::createImageView(m_positionImage, m_positionFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_positionImageView);
    
    m_normalFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    Tools::createImageAndMemoryThenBind(m_normalFormat, width, height, 1, 1,
                                 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                 VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 m_normalImage, m_normalMemory);
    Tools::createImageView(m_normalImage, m_normalFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_normalImageView);
    
    m_albedoFormat = VK_FORMAT_R8G8B8A8_UNORM; // VK_FORMAT_B8G8R8A8_UNORM
    Tools::createImageAndMemoryThenBind(m_albedoFormat, width, height, 1, 1,
                                 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                 VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 m_albedoImage, m_albedoMemory);
    Tools::createImageView(m_albedoImage, m_albedoFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_albedoImageView);
}

void SubPasses::prepareDescriptorSetLayoutAndPipelineLayout()
{
    // pass 1
    {
        VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
        if ( vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_sceneDescriptorSetLayout) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create descriptorSetLayout!");
        }
        
        createPipelineLayout(&m_sceneDescriptorSetLayout, 1, m_scenePipelineLayout);
    }
    
    // pass 2
    {
        VkDescriptorSetLayoutBinding bindings[4] = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        bindings[3] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings, 4);
        if ( vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_compositeDescriptorSetLayout) != VK_SUCCESS )
        {
            throw std::runtime_error("failed to create descriptorSetLayout!");
        }
        createPipelineLayout(&m_compositeDescriptorSetLayout, 1, m_compositePipelineLayout);
    }
    
    // pass 3
    {
        VkDescriptorSetLayoutBinding bindings[3] = {};
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        createDescriptorSetLayout(bindings, 3);
        createPipelineLayout();
    }
}

void SubPasses::prepareDescriptorSetAndWrite()
{
    VkDescriptorPoolSize poolSizes[3] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 3;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    poolSizes[2].descriptorCount = 4;
    
    createDescriptorPool(poolSizes, 3, 3);
    
    // pass 1
    {
        createDescriptorSet(&m_sceneDescriptorSetLayout, 1, m_sceneDescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_sceneUniformBuffer;
        
        VkWriteDescriptorSet write = Tools::getWriteDescriptorSet(m_sceneDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
    }
    
    // pass 2
    {
        createDescriptorSet(&m_compositeDescriptorSetLayout, 1, m_compositeDescriptorSet);
        
        VkDescriptorImageInfo info1 = {};
        info1.sampler = VK_NULL_HANDLE;
        info1.imageView = m_positionImageView;
        info1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkDescriptorImageInfo info2 = {};
        info2.sampler = VK_NULL_HANDLE;
        info2.imageView = m_normalImageView;
        info2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkDescriptorImageInfo info3 = {};
        info3.sampler = VK_NULL_HANDLE;
        info3.imageView = m_albedoImageView;
        info3.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkDescriptorBufferInfo info4 = {};
        info4.offset = 0;
        info4.range = sizeof(ViewPosAndLights);
        info4.buffer = m_lightUniformBuffer;
        
        VkWriteDescriptorSet writes[4] = {};
        writes[0] = Tools::getWriteDescriptorSet(m_compositeDescriptorSet, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0, &info1);
        writes[1] = Tools::getWriteDescriptorSet(m_compositeDescriptorSet, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, &info2);
        writes[2] = Tools::getWriteDescriptorSet(m_compositeDescriptorSet, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, &info3);
        writes[3] = Tools::getWriteDescriptorSet(m_compositeDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &info4);
        vkUpdateDescriptorSets(m_device, 4, writes, 0, nullptr);
    }
    
    // pass 3
    {
        createDescriptorSet(m_transparentDescriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_sceneUniformBuffer;
        
        VkDescriptorImageInfo info1 = {};
        info1.sampler = VK_NULL_HANDLE;
        info1.imageView = m_positionImageView;
        info1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkDescriptorImageInfo imageInfo = m_pGlassTexture->getDescriptorImageInfo();
        
        VkWriteDescriptorSet writes[3] = {};
        writes[0] = Tools::getWriteDescriptorSet(m_transparentDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_transparentDescriptorSet, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, &info1);
        writes[2] = Tools::getWriteDescriptorSet(m_transparentDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo);
        vkUpdateDescriptorSets(m_device, 3, writes, 0, nullptr);
    }
}

void SubPasses::createGraphicsPipeline()
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

//    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
//    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    
    std::array<VkPipelineColorBlendAttachmentState, 4> colorBlendAttachments;
    colorBlendAttachments[0] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    colorBlendAttachments[1] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    colorBlendAttachments[2] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    colorBlendAttachments[3] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(static_cast<uint32_t>(colorBlendAttachments.size()), colorBlendAttachments.data());
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_scenePipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    
    createInfo.pVertexInputState = m_sceneLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    
    // subpass 1
    createInfo.subpass = 0;
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "subpasses/gbuffer.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "subpasses/gbuffer.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_scenePipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // subpass 2
    createInfo.subpass = 1;
    VkPipelineVertexInputStateCreateInfo emptyInputState = {};
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.pVertexInputState = &emptyInputState;
    createInfo.layout = m_compositePipelineLayout;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    depthStencil.depthWriteEnable = VK_FALSE;
    colorBlend.attachmentCount =  1;
    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "subpasses/composition.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "subpasses/composition.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_compositePipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // subpass 3
    createInfo.subpass = 2;
    createInfo.pVertexInputState = m_transparentLoader.getPipelineVertexInputState();
    createInfo.layout = m_pipelineLayout;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment1 = Tools::getPipelineColorBlendAttachmentState(VK_TRUE);
    VkPipelineColorBlendStateCreateInfo colorBlend1 = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment1);
    createInfo.pColorBlendState = &colorBlend1;
    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "subpasses/transparent.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "subpasses/transparent.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_transparentPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void SubPasses::updateRenderData()
{
//    Uniform mvp = {};
//    mvp.modelMatrix = glm::mat4(1.0f);
//    mvp.viewMatrix = m_camera.m_viewMat;
//    mvp.projectionMatrix = m_camera.m_projMat;
//    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void SubPasses::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_scenePipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    // pass 1
    m_sceneLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_scenePipelineLayout, 0, 1, &m_sceneDescriptorSet, 0, nullptr);
    m_sceneLoader.draw(commandBuffer);
    
    // pass 2
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipelineLayout, 0, 1, &m_compositeDescriptorSet, 0, NULL);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    
    // pass 3
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_transparentPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_transparentDescriptorSet, 0, NULL);
    m_transparentLoader.draw(commandBuffer);
}

void SubPasses::createAttachmentDescription()
{
    //5个.
    VkAttachmentDescription surfaceAttachmentDescription = Tools::getAttachmentDescription(m_surfaceFormatKHR.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    
    VkAttachmentDescription positionAttachmentDescription = Tools::getAttachmentDescription(m_positionFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    VkAttachmentDescription normalAttachmentDescription = Tools::getAttachmentDescription(m_normalFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    VkAttachmentDescription albedoAttachmentDescription = Tools::getAttachmentDescription(m_albedoFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    VkAttachmentDescription depthAttachmentDescription = Tools::getAttachmentDescription(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    m_attachmentDescriptions.push_back(surfaceAttachmentDescription);
    m_attachmentDescriptions.push_back(positionAttachmentDescription);
    m_attachmentDescriptions.push_back(normalAttachmentDescription);
    m_attachmentDescriptions.push_back(albedoAttachmentDescription);
    m_attachmentDescriptions.push_back(depthAttachmentDescription);
}

void SubPasses::createRenderPass()
{
    VkAttachmentReference attachmentReference[5] = {};
    attachmentReference[0].attachment = 0;
    attachmentReference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentReference[1].attachment = 1;
    attachmentReference[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentReference[2].attachment = 2;
    attachmentReference[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentReference[3].attachment = 3;
    attachmentReference[3].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentReference[4].attachment = 4;
    attachmentReference[4].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference inputAttachmentReference[3];
    inputAttachmentReference[0].attachment = 1;
    inputAttachmentReference[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputAttachmentReference[1].attachment = 2;
    inputAttachmentReference[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputAttachmentReference[2].attachment = 3;
    inputAttachmentReference[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
    VkSubpassDescription subpassDescription[3] = {};
    subpassDescription[0].flags = 0;
    subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[0].inputAttachmentCount = 0;
    subpassDescription[0].pInputAttachments = nullptr;
    subpassDescription[0].colorAttachmentCount = 4;
    subpassDescription[0].pColorAttachments = attachmentReference;
    subpassDescription[0].pResolveAttachments = nullptr;
    subpassDescription[0].pDepthStencilAttachment = &attachmentReference[4];
    subpassDescription[0].preserveAttachmentCount = 0;
    subpassDescription[0].pPreserveAttachments = nullptr;
    
    subpassDescription[1].flags = 0;
    subpassDescription[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[1].inputAttachmentCount = 3;
    subpassDescription[1].pInputAttachments = inputAttachmentReference;
    subpassDescription[1].colorAttachmentCount = 1;
    subpassDescription[1].pColorAttachments = attachmentReference;
    subpassDescription[1].pResolveAttachments = nullptr;
    subpassDescription[1].pDepthStencilAttachment = nullptr;
    subpassDescription[1].preserveAttachmentCount = 0;
    subpassDescription[1].pPreserveAttachments = nullptr;
    
    subpassDescription[2].flags = 0;
    subpassDescription[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[2].inputAttachmentCount = 1;
    subpassDescription[2].pInputAttachments = inputAttachmentReference;
    subpassDescription[2].colorAttachmentCount = 1;
    subpassDescription[2].pColorAttachments = attachmentReference;
    subpassDescription[2].pResolveAttachments = nullptr;
    subpassDescription[2].pDepthStencilAttachment = nullptr;
    subpassDescription[2].preserveAttachmentCount = 0;
    subpassDescription[2].pPreserveAttachments = nullptr;
    
    VkSubpassDependency dependencies[4] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    dependencies[2].srcSubpass = 1;
    dependencies[2].dstSubpass = 2;
    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[3].srcSubpass = 2;
    dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[3].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[3].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.attachmentCount = static_cast<uint32_t>(m_attachmentDescriptions.size());
    createInfo.pAttachments = m_attachmentDescriptions.data();
    createInfo.subpassCount = 3;
    createInfo.pSubpasses = subpassDescription;
    createInfo.dependencyCount = 4;
    createInfo.pDependencies = dependencies;
    
    if( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create renderpass!");
    }
}

std::vector<VkImageView> SubPasses::getAttachmentsImageViews(size_t i)
{
    std::vector<VkImageView> attachments = {};
    attachments.push_back(m_swapchainImageViews[i]);
    attachments.push_back(m_positionImageView);
    attachments.push_back(m_normalImageView);
    attachments.push_back(m_albedoImageView);
    attachments.push_back(m_depthImageView);
    return attachments;
}

std::vector<VkClearValue> SubPasses::getClearValue()
{
    std::vector<VkClearValue> clearValues = {};
    VkClearValue color1 = {};
    color1.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkClearValue color2 = {};
    color2.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkClearValue color3 = {};
    color3.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkClearValue color4 = {};
    color4.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkClearValue depth = {};
    depth.depthStencil = {1.0f, 0};
    
    clearValues.push_back(color1);
    clearValues.push_back(color2);
    clearValues.push_back(color3);
    clearValues.push_back(color4);
    clearValues.push_back(depth);
    return clearValues;
}


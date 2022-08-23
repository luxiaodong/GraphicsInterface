
#include "deferredmutisampling.h"

DeferredMutiSampling::DeferredMutiSampling(std::string title) : Application(title)
{
}

DeferredMutiSampling::~DeferredMutiSampling()
{}

void DeferredMutiSampling::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createDeferredRenderPass();
    createDeferredFrameBuffer();
    
    createGraphicsPipeline();
}

void DeferredMutiSampling::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.setPosition(glm::vec3(2.15f, 0.3f, -8.75f));
    m_camera.setRotation(glm::vec3(-0.75f, 12.5f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void DeferredMutiSampling::setEnabledFeatures()
{
    if(m_deviceFeatures.sampleRateShading)
    {
        m_deviceEnabledFeatures.sampleRateShading = VK_TRUE;
    }
}

void DeferredMutiSampling::setSampleCount()
{
    m_deferredSampleCount = Tools::getMaxUsableSampleCount();
}

void DeferredMutiSampling::clear()
{
    vkDestroyPipeline(m_device, m_mrtPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_mrtDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_mrtPipelineLayout, nullptr);
    vkFreeMemory(m_device, m_mrtUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_mrtUniformBuffer, nullptr);

    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkFreeMemory(m_device, m_lightUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_lightUniformBuffer, nullptr);

    vkDestroyRenderPass(m_device, m_gbufferRenderPass, nullptr);
    vkDestroyFramebuffer(m_device, m_gbufferFramebuffer, nullptr);
    
    for(uint32_t i = 0; i < 3; ++i)
    {
        vkDestroyImageView(m_device, m_gbufferColorImageView[i], nullptr);
        vkDestroyImage(m_device, m_gbufferColorImage[i], nullptr);
        vkFreeMemory(m_device, m_gbufferColorMemory[i], nullptr);
    }
    
    vkDestroySampler(m_device, m_gbufferColorSample, nullptr);
    
    vkDestroyImageView(m_device, m_gbufferDepthImageView, nullptr);
    vkDestroyImage(m_device, m_gbufferDepthImage, nullptr);
    vkFreeMemory(m_device, m_gbufferDepthMemory, nullptr);
    
    
    m_pObjectColor->clear();
    delete m_pObjectColor;
    m_pObjectNormal->clear();
    delete m_pObjectNormal;
    m_pFloorColor->clear();
    delete m_pFloorColor;
    m_pFloorNormal->clear();
    delete m_pFloorNormal;
    m_floorLoader.clear();
    m_objectLoader.clear();
    Application::clear();
}

void DeferredMutiSampling::prepareVertex()
{
    uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY;
    m_floorLoader.loadFromFile(Tools::getModelPath() + "deferred_box.gltf", m_graphicsQueue, flags);
    m_floorLoader.createVertexAndIndexBuffer();
    
    m_objectLoader.loadFromFile(Tools::getModelPath() + "armor/armor.gltf", m_graphicsQueue, flags);
    m_objectLoader.createVertexAndIndexBuffer();
    m_objectLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal, VertexComponent::Tangent});
    
    m_pObjectColor = Texture::loadTextrue2D(Tools::getModelPath() +  "armor/colormap_rgba.ktx", m_graphicsQueue);
    m_pObjectNormal = Texture::loadTextrue2D(Tools::getModelPath() +  "armor/normalmap_rgba.ktx", m_graphicsQueue);
    m_pFloorColor = Texture::loadTextrue2D(Tools::getTexturePath() +  "stonefloor02_color_rgba.ktx", m_graphicsQueue);
    m_pFloorNormal = Texture::loadTextrue2D(Tools::getTexturePath() +  "stonefloor02_normal_rgba.ktx", m_graphicsQueue);
}

void DeferredMutiSampling::prepareUniform()
{
    // mrt
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_mrtUniformBuffer, m_mrtUniformMemory);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::mat4(1.0f);
    mvp.instancePos[0] = glm::vec4(0.0f);
    mvp.instancePos[1] = glm::vec4(-4.0f, 0.0, -4.0f, 0.0f);
    mvp.instancePos[2] = glm::vec4(4.0f, 0.0, -4.0f, 0.0f);
    Tools::mapMemory(m_mrtUniformMemory, sizeof(Uniform), &mvp);
    
    // light
    uniformSize = sizeof(LightData);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_lightUniformBuffer, m_lightUniformMemory);
    
}

void DeferredMutiSampling::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_mrtDescriptorSetLayout));
        createPipelineLayout(&m_mrtDescriptorSetLayout, 1, m_mrtPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);
        bindings[3] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4);
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout();
    }
}

void DeferredMutiSampling::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 3;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 7;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 3);
    
    {
        createDescriptorSet(&m_mrtDescriptorSetLayout, 1, m_floorDescriptorSet);

        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_mrtUniformBuffer;

        VkDescriptorImageInfo imageInfo1 = m_pFloorColor->getDescriptorImageInfo();
        VkDescriptorImageInfo imageInfo2 = m_pFloorNormal->getDescriptorImageInfo();

        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_floorDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_floorDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo1);
        writes[2] = Tools::getWriteDescriptorSet(m_floorDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(&m_mrtDescriptorSetLayout, 1, m_objectDescriptorSet);

        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_mrtUniformBuffer;

        VkDescriptorImageInfo imageInfo1 = m_pObjectColor->getDescriptorImageInfo();
        VkDescriptorImageInfo imageInfo2 = m_pObjectNormal->getDescriptorImageInfo();

        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo1);
        writes[2] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        
        VkDescriptorImageInfo imageInfo[3];
        for(uint32_t i = 0; i < 3; ++i)
        {
            imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo[i].imageView = m_gbufferColorImageView[i];
            imageInfo[i].sampler = m_gbufferColorSample;
        }
    
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_lightUniformBuffer;

        std::array<VkWriteDescriptorSet, 4> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo[0]);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo[1]);
        writes[2] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageInfo[2]);
        writes[3] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &bufferInfo1);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void DeferredMutiSampling::createGraphicsPipeline()
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
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(m_deferredSampleCount);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    
    multisample.alphaToCoverageEnable = VK_TRUE;
    multisample.sampleShadingEnable = VK_TRUE;
    multisample.minSampleShading = 0.25;

    std::array<VkPipelineColorBlendAttachmentState, 3> colorBlendAttachments;
    colorBlendAttachments[0] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    colorBlendAttachments[1] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    colorBlendAttachments[2] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo( static_cast<uint32_t>(colorBlendAttachments.size()), colorBlendAttachments.data());

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
//    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    createInfo.pVertexInputState = m_objectLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    createInfo.layout = m_mrtPipelineLayout;
    createInfo.renderPass = m_gbufferRenderPass;
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "deferredmultisampling/mrt.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "deferredmultisampling/mrt.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_mrtPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // deferred
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend1 = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    createInfo.pColorBlendState = &colorBlend1;
    VkPipelineMultisampleStateCreateInfo multisample1 = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    createInfo.pMultisampleState = &multisample1;
    
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "deferredmultisampling/deferred.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "deferredmultisampling/deferred.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    
    VkSpecializationMapEntry specializationEntry{};
    specializationEntry.constantID = 0;
    specializationEntry.offset = 0;
    specializationEntry.size = sizeof(uint32_t);

    uint32_t specializationData = m_deferredSampleCount;

    VkSpecializationInfo specializationInfo;
    specializationInfo.mapEntryCount = 1;
    specializationInfo.pMapEntries = &specializationEntry;
    specializationInfo.dataSize = sizeof(specializationData);
    specializationInfo.pData = &specializationData;
    shaderStages[1].pSpecializationInfo = &specializationInfo;
    
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void DeferredMutiSampling::updateRenderData()
{
    LightData lightData;
    // White
    lightData.lights[0].position = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    lightData.lights[0].color = glm::vec3(1.5f);
    lightData.lights[0].radius = 15.0f * 0.25f;
    // Red
    lightData.lights[1].position = glm::vec4(-2.0f, 0.0f, 0.0f, 0.0f);
    lightData.lights[1].color = glm::vec3(1.0f, 0.0f, 0.0f);
    lightData.lights[1].radius = 15.0f;
    // Blue
    lightData.lights[2].position = glm::vec4(2.0f, -1.0f, 0.0f, 0.0f);
    lightData.lights[2].color = glm::vec3(0.0f, 0.0f, 2.5f);
    lightData.lights[2].radius = 5.0f;
    // Yellow
    lightData.lights[3].position = glm::vec4(0.0f, -0.9f, 0.5f, 0.0f);
    lightData.lights[3].color = glm::vec3(1.0f, 1.0f, 0.0f);
    lightData.lights[3].radius = 2.0f;
    // Green
    lightData.lights[4].position = glm::vec4(0.0f, -0.5f, 0.0f, 0.0f);
    lightData.lights[4].color = glm::vec3(0.0f, 1.0f, 0.2f);
    lightData.lights[4].radius = 5.0f;
    // Yellow
    lightData.lights[5].position = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    lightData.lights[5].color = glm::vec3(1.0f, 0.7f, 0.3f);
    lightData.lights[5].radius = 25.0f;

    float timer = 0.1f;
    lightData.lights[0].position.x = sin(glm::radians(360.0f * timer)) * 5.0f;
    lightData.lights[0].position.z = cos(glm::radians(360.0f * timer)) * 5.0f;
    lightData.lights[1].position.x = -4.0f + sin(glm::radians(360.0f * timer) + 45.0f) * 2.0f;
    lightData.lights[1].position.z =  0.0f + cos(glm::radians(360.0f * timer) + 45.0f) * 2.0f;
    lightData.lights[2].position.x = 4.0f + sin(glm::radians(360.0f * timer)) * 2.0f;
    lightData.lights[2].position.z = 0.0f + cos(glm::radians(360.0f * timer)) * 2.0f;
    lightData.lights[4].position.x = 0.0f + sin(glm::radians(360.0f * timer + 90.0f)) * 5.0f;
    lightData.lights[4].position.z = 0.0f - cos(glm::radians(360.0f * timer + 45.0f)) * 5.0f;
    lightData.lights[5].position.x = 0.0f + sin(glm::radians(-360.0f * timer + 135.0f)) * 10.0f;
    lightData.lights[5].position.z = 0.0f - cos(glm::radians(-360.0f * timer - 45.0f)) * 10.0f;
    timer += 0.001f;

    lightData.viewPos = glm::vec4(m_camera.m_position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);
    lightData.debugDisplayTarget = 0;
    Tools::mapMemory(m_lightUniformMemory, sizeof(lightData), &lightData);
}

void DeferredMutiSampling::recordRenderCommand(const VkCommandBuffer commandBuffer)
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

void DeferredMutiSampling::createOtherBuffer()
{
    m_gbufferWidth = m_swapchainExtent.width;
    m_gbufferHeight = m_swapchainExtent.height;
    
    m_gbufferColorFormat[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_gbufferColorFormat[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_gbufferColorFormat[2] = VK_FORMAT_R8G8B8A8_UNORM;
    m_gbufferDepthFormat = VK_FORMAT_D32_SFLOAT;
    
    for(uint32_t i = 0; i < 3; ++i)
    {
        Tools::createImageAndMemoryThenBind(m_gbufferColorFormat[i], m_gbufferWidth, m_gbufferHeight, 1, 1, m_deferredSampleCount, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_gbufferColorImage[i], m_gbufferColorMemory[i]);
        
        Tools::createImageView(m_gbufferColorImage[i], m_gbufferColorFormat[i], VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_gbufferColorImageView[i]);
    }
    
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_gbufferColorSample);
    
    // depth
    Tools::createImageAndMemoryThenBind(m_gbufferDepthFormat, m_gbufferWidth, m_gbufferHeight, 1, 1,
                                        m_deferredSampleCount, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_gbufferDepthImage, m_gbufferDepthMemory);
    
    Tools::createImageView(m_gbufferDepthImage, m_gbufferDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_gbufferDepthImageView);
}

void DeferredMutiSampling::createDeferredRenderPass()
{
    std::array<VkAttachmentDescription, 4> attachmentDescription;
    
    for(uint32_t i = 0; i < 3; ++i)
    {
        attachmentDescription[i] = Tools::getAttachmentDescription(m_gbufferColorFormat[i], m_deferredSampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    
    attachmentDescription[3] = Tools::getAttachmentDescription(m_gbufferDepthFormat, m_deferredSampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    VkAttachmentReference colorAttachmentReference[3] = {};
    for(uint32_t i = 0; i < 3; ++i)
    {
        colorAttachmentReference[i].attachment = i;
        colorAttachmentReference[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 3;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 3;
    subpassDescription.pColorAttachments = colorAttachmentReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.attachmentCount = static_cast<uint32_t>(attachmentDescription.size());
    createInfo.pAttachments = attachmentDescription.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    
    VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_gbufferRenderPass) );
}

void DeferredMutiSampling::createDeferredFrameBuffer()
{
    std::vector<VkImageView> attachments;
    for(uint32_t i = 0; i < 3; ++i)
    {
        attachments.push_back(m_gbufferColorImageView[i]);
    }

    attachments.push_back(m_gbufferDepthImageView);
    
    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = m_gbufferRenderPass;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.width = m_gbufferWidth;
    createInfo.height = m_gbufferHeight;
    createInfo.layers = 1;
    
    VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_gbufferFramebuffer));
}

void DeferredMutiSampling::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    std::array<VkClearValue, 4> clearValues = {};
    for(uint32_t i = 0; i < 3; ++i)
    {
        clearValues[i].color = {0.0f, 0.0f, 0.0f, 1.0f};
    }
    
    clearValues[3].depthStencil = {1.0f, 0};
    
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_gbufferRenderPass;
    passBeginInfo.framebuffer = m_gbufferFramebuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = m_gbufferWidth;
    passBeginInfo.renderArea.extent.height = m_gbufferHeight;
    passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = Tools::getViewport(0, 0, m_gbufferWidth, m_gbufferHeight);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = m_gbufferWidth;
    scissor.extent.height = m_gbufferHeight;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mrtPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mrtPipelineLayout, 0, 1, &m_floorDescriptorSet, 0, nullptr);
    m_floorLoader.bindBuffers(commandBuffer);
    m_floorLoader.draw(commandBuffer);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mrtPipelineLayout, 0, 1, &m_objectDescriptorSet, 0, nullptr);
    m_objectLoader.bindBuffers(commandBuffer);
//    m_objectLoader.draw(commandBuffer);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_objectLoader.m_indexData.size()), 3, 0, 0, 0);
    
    vkCmdEndRenderPass(commandBuffer);
}

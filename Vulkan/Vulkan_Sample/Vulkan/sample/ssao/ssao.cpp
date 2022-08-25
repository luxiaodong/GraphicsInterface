
#include "ssao.h"

#define SSAO_KERNEL_SIZE 32
#define SSAO_RADIUS 0.3f
#define SSAO_NOISE_DIM 8

DeferredSsao::DeferredSsao(std::string title) : Application(title)
{
}

DeferredSsao::~DeferredSsao()
{}

void DeferredSsao::init()
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

void DeferredSsao::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.setPosition(glm::vec3(1.0f, 0.75f, 0.0f));
    m_camera.setRotation(glm::vec3(0.0f, 90.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 64.0f);
}

void DeferredSsao::setEnabledFeatures()
{
//    if(m_deviceFeatures.samplerAnisotropy)
//    {
//        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
//    }
}

void DeferredSsao::clear()
{
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    
    vkDestroyPipeline(m_device, m_ssaoPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_ssaoDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_ssaoPipelineLayout, nullptr);
    
    vkDestroyPipeline(m_device, m_gbufferPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_gbufferDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, m_gbufferPipelineLayout, nullptr);

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
    
    vkFreeMemory(m_device, m_objectUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_objectUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_paramsUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_paramsUniformBuffer, nullptr);
    vkFreeMemory(m_device, m_sampleUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_sampleUniformBuffer, nullptr);
    
    m_pNoise->clear();
    delete m_pNoise;
    m_objectLoader.clear();
    Application::clear();
}

void DeferredSsao::prepareVertex()
{
    uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::FlipY;
    
    m_objectLoader.loadFromFile(Tools::getModelPath() + "sponza/sponza.gltf", m_graphicsQueue, flags);
    m_objectLoader.createVertexAndIndexBuffer();
    m_objectLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal});
    m_objectLoader.createDescriptorPoolAndLayout();
    
    // Random noise
    std::vector<glm::vec4> ssaoNoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
    for (uint32_t i = 0; i < static_cast<uint32_t>(ssaoNoise.size()); i++)
    {
        ssaoNoise[i] = glm::vec4(Tools::random01() * 2.0f - 1.0f, Tools::random01() * 2.0f - 1.0f, 0.0f, 0.0f);
    }

    m_pNoise = Texture::loadTextrue2D(ssaoNoise.data(), ssaoNoise.size() * sizeof(glm::vec4), SSAO_NOISE_DIM, SSAO_NOISE_DIM, VK_FORMAT_R32G32B32A32_SFLOAT, m_graphicsQueue);
}

void DeferredSsao::prepareUniform()
{
    // object
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_objectUniformBuffer, m_objectUniformMemory);

    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::mat4(1.0f);
    Tools::mapMemory(m_objectUniformMemory, sizeof(Uniform), &mvp);
    
    // ssao sample
    uniformSize = sizeof(glm::vec4) * SSAO_KERNEL_SIZE;
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_sampleUniformBuffer, m_sampleUniformMemory);
    
    std::vector<glm::vec4> ssaoSample( SSAO_KERNEL_SIZE );
    for(uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        glm::vec3 sample(Tools::random01()*2.0 - 1.0, Tools::random01()*2.0 - 1.0, Tools::random01()*2.0 - 1.0);
        sample = glm::normalize(sample);
        sample *= Tools::random01();
        float scale = i*1.0f/float(SSAO_KERNEL_SIZE);
        scale = Tools::lerp(0.1f, 1.0f, scale * scale);
        ssaoSample[i] = glm::vec4(sample * scale, 0.0f);
    }
    
    Tools::mapMemory(m_sampleUniformMemory, uniformSize, ssaoSample.data());
    
    // ssao params
    uniformSize = sizeof(SsaoParams);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_paramsUniformBuffer, m_paramsUniformMemory);
    
    SsaoParams params = {};
    params.projection = m_camera.m_projMat;
    params.ssao = false;
    params.ssaoOnly = false;
    params.ssaoBlur = false;
    Tools::mapMemory(m_paramsUniformMemory, sizeof(SsaoParams), &params);
}

void DeferredSsao::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        std::array<VkDescriptorSetLayoutBinding, 1> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_gbufferDescriptorSetLayout));
        
        const std::vector<VkDescriptorSetLayout> setLayouts = {m_gbufferDescriptorSetLayout, GltfLoader::m_imageDescriptorSetLayout};
        createPipelineLayout(setLayouts.data(), 2, m_gbufferPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 5> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        bindings[3] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);
        bindings[4] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_ssaoDescriptorSetLayout));
        createPipelineLayout(&m_ssaoDescriptorSetLayout, 1, m_ssaoPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 6> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        bindings[3] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3);
        bindings[4] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4);
        bindings[5] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5);
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout();
    }
}

void DeferredSsao::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 8;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 3);
    
    {
        createDescriptorSet(&m_gbufferDescriptorSetLayout, 1, m_objectDescriptorSet);

        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_objectUniformBuffer;

        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_objectDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo1);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(&m_ssaoDescriptorSetLayout, 1, m_ssaoDescriptorSet);
        
        VkDescriptorImageInfo imageInfo1 = {};
        imageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo1.imageView = m_gbufferColorImageView[0];
        imageInfo1.sampler = m_gbufferColorSample;
        
        VkDescriptorImageInfo imageInfo2 = {};
        imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo2.imageView = m_gbufferColorImageView[1];
        imageInfo2.sampler = m_gbufferColorSample;
        
        VkDescriptorImageInfo imageInfo3 = {};
        imageInfo3.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo3.imageView = m_gbufferColorImageView[2];
        imageInfo3.sampler = m_gbufferColorSample;
        
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_sampleUniformBuffer;
        
        VkDescriptorBufferInfo bufferInfo2 = {};
        bufferInfo2.offset = 0;
        bufferInfo2.range = VK_WHOLE_SIZE;
        bufferInfo2.buffer = m_paramsUniformBuffer;

        std::array<VkWriteDescriptorSet, 5> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_ssaoDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo1);
        writes[1] = Tools::getWriteDescriptorSet(m_ssaoDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo2);
        writes[2] = Tools::getWriteDescriptorSet(m_ssaoDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo3);
        writes[3] = Tools::getWriteDescriptorSet(m_ssaoDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &bufferInfo1);
        writes[4] = Tools::getWriteDescriptorSet(m_ssaoDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &bufferInfo2);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = VK_WHOLE_SIZE;
        bufferInfo1.buffer = m_paramsUniformBuffer;
        
        VkDescriptorImageInfo imageInfo[3];
        for(uint32_t i = 0; i < 3; ++i)
        {
            imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo[i].imageView = m_gbufferColorImageView[i];
            imageInfo[i].sampler = m_gbufferColorSample;
        }

        std::array<VkWriteDescriptorSet, 6> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo[0]);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo[1]);
        writes[2] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo[2]);
        writes[3] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageInfo[1]);
        writes[4] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &imageInfo[2]);
        writes[5] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5, &bufferInfo1);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void DeferredSsao::createGraphicsPipeline()
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
    
    createInfo.layout = m_gbufferPipelineLayout;
    createInfo.renderPass = m_gbufferRenderPass;
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "ssao/gbuffer.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "ssao/gbuffer.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_gbufferPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // deferred
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend1 = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
    createInfo.pColorBlendState = &colorBlend1;
    
    rasterization.cullMode = VK_CULL_MODE_FRONT_BIT;
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "ssao/fullscreen.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "ssao/composition.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void DeferredSsao::updateRenderData()
{
}

void DeferredSsao::recordRenderCommand(const VkCommandBuffer commandBuffer)
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

void DeferredSsao::createOtherBuffer()
{
    m_gbufferWidth = m_swapchainExtent.width;
    m_gbufferHeight = m_swapchainExtent.height;
    
    m_gbufferColorFormat[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_gbufferColorFormat[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_gbufferColorFormat[2] = VK_FORMAT_R8G8B8A8_UNORM;
    m_gbufferDepthFormat = VK_FORMAT_D32_SFLOAT;
    
    for(uint32_t i = 0; i < 3; ++i)
    {
        Tools::createImageAndMemoryThenBind(m_gbufferColorFormat[i], m_gbufferWidth, m_gbufferHeight, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_gbufferColorImage[i], m_gbufferColorMemory[i]);
        
        Tools::createImageView(m_gbufferColorImage[i], m_gbufferColorFormat[i], VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, m_gbufferColorImageView[i]);
    }
    
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_gbufferColorSample);
    
    // depth
    Tools::createImageAndMemoryThenBind(m_gbufferDepthFormat, m_gbufferWidth, m_gbufferHeight, 1, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_gbufferDepthImage, m_gbufferDepthMemory);
    
    Tools::createImageView(m_gbufferDepthImage, m_gbufferDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, m_gbufferDepthImageView);
}

void DeferredSsao::createDeferredRenderPass()
{
    std::array<VkAttachmentDescription, 4> attachmentDescription;
    
    for(uint32_t i = 0; i < 3; ++i)
    {
        attachmentDescription[i] = Tools::getAttachmentDescription(m_gbufferColorFormat[i], VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    
    attachmentDescription[3] = Tools::getAttachmentDescription(m_gbufferDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
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

void DeferredSsao::createDeferredFrameBuffer()
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

void DeferredSsao::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
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
    passBeginInfo.renderArea.extent = m_swapchainExtent;
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
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufferPipeline);

    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_gbufferPipelineLayout, 0, 1, &m_objectDescriptorSet, 0, nullptr);
    m_objectLoader.bindBuffers(commandBuffer);
    m_objectLoader.draw(commandBuffer, m_gbufferPipelineLayout, 4);
    
    vkCmdEndRenderPass(commandBuffer);
}


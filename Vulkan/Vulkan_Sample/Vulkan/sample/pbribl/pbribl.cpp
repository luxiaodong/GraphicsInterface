

#include "pbribl.h"
#include <stdlib.h>
#include <random>

PbrIbl::PbrIbl(std::string title) : Application(title)
{
}

PbrIbl::~PbrIbl()
{}

void PbrIbl::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
    
    selectPbrMaterial();
}

void PbrIbl::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.setPosition(glm::vec3(0.55f, 0.85f, 12.0f));
    m_camera.setRotation(glm::vec3(-3.75f, 180.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 256.0f);
}

void PbrIbl::setEnabledFeatures()
{
}

void PbrIbl::clear()
{
    vkDestroyPipeline(m_device, m_skyboxPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_skyboxPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_skyboxDescriptorSetLayout, nullptr);
    
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_lightBuffer, nullptr);
    vkFreeMemory(m_device, m_lightMemory, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    
    
    vkFreeMemory(m_device, m_irrMemory, nullptr);
    vkDestroyImage(m_device, m_irrImage, nullptr);
    vkDestroyImageView(m_device, m_irrImageView, nullptr);
    vkDestroySampler(m_device, m_irrSampler, nullptr);
    
    vkFreeMemory(m_device, m_lutMemory, nullptr);
    vkDestroyImage(m_device, m_lutImage, nullptr);
    vkDestroyImageView(m_device, m_lutImageView, nullptr);

    m_pEnvCube->clear();
    delete m_pEnvCube;
    m_gltfLoader.clear();
    m_skyboxLoader.clear();
    Application::clear();
}

void PbrIbl::prepareVertex()
{
    const uint32_t flags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::FlipY;
    std::vector<std::string> fileNames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
    m_gltfLoader.loadFromFile(Tools::getModelPath() + fileNames[0], m_graphicsQueue, flags);
    m_gltfLoader.createVertexAndIndexBuffer();
    m_gltfLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
    
    m_skyboxLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue, flags);
    m_skyboxLoader.createVertexAndIndexBuffer();
    m_skyboxLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::UV});
    
    // HDR cubemap
    m_pEnvCube = Texture::loadTextrue2D(Tools::getTexturePath() +  "hdr/pisa_cube.ktx", m_graphicsQueue, VK_FORMAT_R16G16B16A16_SFLOAT, TextureCopyRegion::Cube);
    
    createIrrImage();
}

void PbrIbl::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    VkDeviceSize LightParamsSize = sizeof(LightParams);
    Tools::createBufferAndMemoryThenBind(LightParamsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_lightBuffer, m_lightMemory);
}

void PbrIbl::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(bindings.data(), static_cast<uint32_t>(bindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_skyboxDescriptorSetLayout));
        createPipelineLayout(&m_skyboxDescriptorSetLayout, 1, m_skyboxPipelineLayout);
    }
    
    {
        std::array<VkPushConstantRange, 2> pushConstantRange = {};
        pushConstantRange[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange[0].offset = 0;
        pushConstantRange[0].size = sizeof(glm::vec3);
        pushConstantRange[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange[1].offset = sizeof(glm::vec3);
        pushConstantRange[1].size = sizeof(PbrMaterial::PushBlock);

        std::array<VkDescriptorSetLayoutBinding, 2> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
        createPipelineLayout(pushConstantRange.data(), static_cast<uint32_t>(pushConstantRange.size()));
    }
}

void PbrIbl::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);

    {
        createDescriptorSet(&m_skyboxDescriptorSetLayout, 1, m_skyboxDescriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
     
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = sizeof(LightParams);
        bufferInfo1.buffer = m_lightBuffer;
        
        VkDescriptorImageInfo imageInfo = m_pEnvCube->getDescriptorImageInfo();
//        VkDescriptorImageInfo imageInfo = {};
//        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//        imageInfo.imageView = m_irrImageView;
//        imageInfo.sampler = m_irrSampler;
        
        std::array<VkWriteDescriptorSet, 3> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &bufferInfo1);
        writes[2] = Tools::getWriteDescriptorSet(m_skyboxDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorBufferInfo bufferInfo1 = {};
        bufferInfo1.offset = 0;
        bufferInfo1.range = sizeof(LightParams);
        bufferInfo1.buffer = m_lightBuffer;

        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &bufferInfo1);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void PbrIbl::createGraphicsPipeline()
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
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
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

    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "pbrbasic/pbr.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "pbrbasic/pbr.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //skybox
    createInfo.layout = m_skyboxPipelineLayout;
    createInfo.pVertexInputState = m_skyboxLoader.getPipelineVertexInputState();
    rasterization.cullMode = VK_CULL_MODE_NONE;
    
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthTestEnable = VK_FALSE;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "pbribl/skybox.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "pbribl/skybox.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_skyboxPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void PbrIbl::updateRenderData()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    mvp.camPos = m_camera.m_position * -1.0f;
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
    
    // skybox
    mvp.modelMatrix = glm::mat4(glm::mat3(m_camera.m_viewMat));
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
    
    VkDeviceSize LightParamsSize = sizeof(LightParams);
    LightParams params = {};
    const float p = 15.0f;
    params.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
    params.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
    params.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
    params.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);
    params.exposure = 4.5f;
    params.gamma = 2.2f;
    Tools::mapMemory(m_lightMemory, LightParamsSize, &params);
}

void PbrIbl::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // render skybox
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);
    m_skyboxLoader.bindBuffers(commandBuffer);
    m_skyboxLoader.draw(commandBuffer);

    
    // render object
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
//    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
//    m_gltfLoader.bindBuffers(commandBuffer);
//
//    int GRID_DIM = 7;
//    for (uint32_t y = 0; y < GRID_DIM; y++)
//    {
//        for (uint32_t x = 0; x < GRID_DIM; x++)
//        {
//            glm::vec3 pos = glm::vec3(float(x - (GRID_DIM / 2.0f)) * 2.5f, 0.0f, float(y - (GRID_DIM / 2.0f)) * 2.5f);
//            vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
//            m_pbrMaterial.params.metallic = glm::clamp((float)x / (float)(GRID_DIM - 1), 0.1f, 1.0f);
//            m_pbrMaterial.params.roughness = glm::clamp((float)y / (float)(GRID_DIM - 1), 0.05f, 1.0f);
//            vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(PbrMaterial::PushBlock), &m_pbrMaterial);
//            m_gltfLoader.draw(commandBuffer);
//        }
//    }
}

void PbrIbl::createIrrImage()
{
    uint32_t width = m_pEnvCube->m_width;
    uint32_t height = m_pEnvCube->m_height;
    VkFormat format = m_pEnvCube->m_fromat;
    
    m_irrMaxLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1.0);
    m_irrMaxLevels = 1;
    Tools::createImageAndMemoryThenBind(format, width, height, m_irrMaxLevels, 6,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_irrImage, m_irrMemory, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    
    Tools::createImageView(m_irrImage, format, VK_IMAGE_ASPECT_COLOR_BIT, m_irrMaxLevels, 6, m_irrImageView, VK_IMAGE_VIEW_TYPE_CUBE);
    
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, m_irrMaxLevels, m_irrSampler);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 6;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = m_irrMaxLevels;
    
    Tools::setImageLayout(cmd, m_irrImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange);
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
}

void PbrIbl::betweenInitAndLoop()
{
//    generateBrdfLut();
//    generateIrradianceCube();
//    vkDeviceWaitIdle(m_device);
}

void PbrIbl::generateBrdfLut()
{
    int width = 1024;
    int height = 1024;
    
    VkFormat format = VK_FORMAT_R16G16_SFLOAT;
    VkImage frameImage;
    VkDeviceMemory frameMemory;
    VkImageView frameImageView;
    {
        Tools::createImageAndMemoryThenBind(format, width, height, 1, 1,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            frameImage, frameMemory);
        
        Tools::createImageView(frameImage, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, frameImageView);
        
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tools::setImageLayout(cmd, frameImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    }
    
    VkRenderPass lutRenderPass;
    {
        VkAttachmentDescription attachmentDescription;
        attachmentDescription.flags = 0;
        attachmentDescription.format = format;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference attachmentReference;
        attachmentReference.attachment = 0;
        attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
     
        VkSubpassDescription subpassDescription = {};
        subpassDescription.flags = 0;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &attachmentReference;
        subpassDescription.pResolveAttachments = nullptr;
        subpassDescription.pDepthStencilAttachment = nullptr;
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
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &attachmentDescription;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassDescription;
        createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        createInfo.pDependencies = dependencies.data();
        
        VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &lutRenderPass) );
    }
    
    VkFramebuffer lutFrameBuffer;
    {
        VkFramebufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = lutRenderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &frameImageView;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;
        
        VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &lutFrameBuffer));
    }
    
    VkPipelineLayout lutPipelineLayout;
    {
        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        vkCreatePipelineLayout(m_device, &createInfo, nullptr, &lutPipelineLayout);
    }
    
    VkPipeline lutPipeline;
    {
        VkViewport vp;
        vp.x = 0;
        vp.y = 0;
        vp.width = width;
        vp.height = height;
        vp.minDepth = 0;
        vp.maxDepth = 1;
        
        VkRect2D scissor;
        scissor.offset = {0, 0};
        scissor.extent.width = width;
        scissor.extent.height = height;
        
        VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "pbribl/genbrdflut.vert.spv");
        VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "pbribl/genbrdflut.frag.spv");
        VkPipelineShaderStageCreateInfo vertShader = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
        VkPipelineShaderStageCreateInfo fragShader = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
        
        VkPipelineVertexInputStateCreateInfo vertexInput = {};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = Tools::getPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
        VkPipelineViewportStateCreateInfo viewport = {};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.flags = 0;
        viewport.viewportCount = 1;
        viewport.pViewports = &vp;
        viewport.scissorCount = 1;
        viewport.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
        VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.push_back(vertShader);
        shaderStages.push_back(fragShader);

        VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(shaderStages, lutPipelineLayout, lutRenderPass);
        createInfo.pVertexInputState = &vertexInput;
        createInfo.pInputAssemblyState = &inputAssembly;
        createInfo.pTessellationState = nullptr;
        createInfo.pViewportState = &viewport;
        createInfo.pRasterizationState = &rasterization;
        createInfo.pMultisampleState = &multisample;
        createInfo.pDepthStencilState = &depthStencil;
        createInfo.pColorBlendState = &colorBlend;
        createInfo.subpass = 0;
        
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &lutPipeline));
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }
    
    VkCommandBuffer commandBuffer = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkClearValue clearColor = {0, 0, 0, 0};
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = lutRenderPass;
    passBeginInfo.framebuffer = lutFrameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = width;
    passBeginInfo.renderArea.extent.height = height;
    passBeginInfo.clearValueCount = 1;
    passBeginInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lutPipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    Tools::flushCommandBuffer(commandBuffer, m_graphicsQueue, true);
    
    Tools::saveImage(frameImage, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, width, height, "screenshot.png");
    
    vkDestroyPipeline(m_device, lutPipeline, nullptr);
    vkDestroyFramebuffer(m_device, lutFrameBuffer, nullptr);
    vkDestroyRenderPass(m_device, lutRenderPass, nullptr);
    vkDestroyPipelineLayout(m_device, lutPipelineLayout, nullptr);

    m_lutImage = frameImage;
    m_lutMemory = frameMemory;
    m_lutImageView = frameImageView;

//    vkFreeMemory(m_device, frameMemory, nullptr);
//    vkDestroyImage(m_device, frameImage, nullptr);
//    vkDestroyImageView(m_device, frameImageView, nullptr);
}

void PbrIbl::generateIrradianceCube()
{
    uint32_t width = m_pEnvCube->m_width;
    uint32_t height = m_pEnvCube->m_height;
    
    VkFormat format = m_pEnvCube->m_fromat;
    VkImage frameImage;
    VkDeviceMemory frameMemory;
    VkImageView frameImageView;
    {
        Tools::createImageAndMemoryThenBind(format, width, height, 1, 1,
                                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            frameImage, frameMemory);
        
        Tools::createImageView(frameImage, format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, frameImageView);
        
        VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Tools::setImageLayout(cmd, frameImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    }
    
    // 渲染管线
    VkRenderPass irrRenderPass;
    {
        VkAttachmentDescription attachmentDescription;
        attachmentDescription.flags = 0;
        attachmentDescription.format = format;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference attachmentReference;
        attachmentReference.attachment = 0;
        attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDescription = {};
        subpassDescription.flags = 0;
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &attachmentReference;
        subpassDescription.pResolveAttachments = nullptr;
        subpassDescription.pDepthStencilAttachment = nullptr;
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
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &attachmentDescription;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassDescription;
        createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        createInfo.pDependencies = dependencies.data();
        
        VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &irrRenderPass));
    }
    
    VkFramebuffer irrFrameBuffer;
    {
        VkFramebufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = irrRenderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &frameImageView;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;
        
        VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &irrFrameBuffer));
    }
    
    VkDescriptorSetLayout irrDescriptorSetLayout;
    {
        VkDescriptorSetLayoutBinding binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &irrDescriptorSetLayout));
    }
    
    struct PushBlock
    {
        glm::mat4 mvp;
        float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
        float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
    } pushBlock;
    
    VkPipelineLayout irrPipelineLayout;
    {
        VkPushConstantRange constantRange = {};
        constantRange.offset = 0;
        constantRange.size = sizeof(PushBlock);
        constantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        
        VkPipelineLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.setLayoutCount = 1;
        createInfo.pSetLayouts = &irrDescriptorSetLayout;
        createInfo.pushConstantRangeCount = 1;
        createInfo.pPushConstantRanges = &constantRange;
        vkCreatePipelineLayout(m_device, &createInfo, nullptr, &irrPipelineLayout);
    }
    
    VkDescriptorPool irrDescriptorPool;
    VkDescriptorSet irrDescriptorSet;
    {
        std::array<VkDescriptorPoolSize, 1> poolSizes;
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = 1;

        VkDescriptorPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.flags = 0;
        createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        createInfo.pPoolSizes = poolSizes.data();
        createInfo.maxSets = 1;
        VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &createInfo, nullptr, &irrDescriptorPool));
        
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = irrDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &irrDescriptorSetLayout;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &irrDescriptorSet));
        
        VkDescriptorImageInfo imageInfo = m_pEnvCube->getDescriptorImageInfo();

        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(irrDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    
    VkPipeline irrPipeline;
    {
        VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "pbribl/filtercube.vert.spv");
        VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "pbribl/irradiancecube.frag.spv");
        VkPipelineShaderStageCreateInfo vertShader = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
        VkPipelineShaderStageCreateInfo fragShader = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
        
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
        VkPipelineColorBlendAttachmentState colorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        shaderStages.push_back(vertShader);
        shaderStages.push_back(fragShader);

        VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(shaderStages, irrPipelineLayout, irrRenderPass);
        createInfo.pVertexInputState = m_skyboxLoader.getPipelineVertexInputState();
        createInfo.pInputAssemblyState = &inputAssembly;
        createInfo.pTessellationState = nullptr;
        createInfo.pViewportState = &viewport;
        createInfo.pRasterizationState = &rasterization;
        createInfo.pMultisampleState = &multisample;
        createInfo.pDepthStencilState = &depthStencil;
        createInfo.pColorBlendState = &colorBlend;
        createInfo.pDynamicState = &dynamic;
        createInfo.subpass = 0;
        
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &irrPipeline));
        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }
    
    VkCommandBuffer commandBuffer = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkClearValue clearColor = {0, 0, 0, 0};
    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = irrRenderPass;
    passBeginInfo.framebuffer = irrFrameBuffer;
    passBeginInfo.renderArea.offset = {0, 0};
    passBeginInfo.renderArea.extent.width = width;
    passBeginInfo.renderArea.extent.height = height;
    passBeginInfo.clearValueCount = 1;
    passBeginInfo.pClearValues = &clearColor;
    
    glm::mat4 projMat = glm::perspective((float)(M_PI/2.0), 1.0f, 0.1f, 256.0f);
    for(uint32_t level = 0; level < m_irrMaxLevels; ++level)
    {
        uint32_t viewWidth = width >> level;
        uint32_t viewHeight = height >> level;
        for(int face = 0; face < 6; ++face)
        {
            glm::mat4 viewMat = glm::mat4(1.0f);
            if(face == 0)
            {
                viewMat = glm::rotate(viewMat, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                viewMat = glm::rotate(viewMat, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            }
            else if(face == 1)
            {
                viewMat = glm::rotate(viewMat, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                viewMat = glm::rotate(viewMat, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            }
            else if(face == 2)
            {
                viewMat = glm::rotate(viewMat, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            }
            else if(face == 3)
            {
                viewMat = glm::rotate(viewMat, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            }
            else if(face == 4)
            {
                viewMat = glm::rotate(viewMat, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            }
            else if(face == 5)
            {
                viewMat = glm::rotate(viewMat, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            }
            
            pushBlock.mvp = projMat * viewMat;
            VkViewport viewport = Tools::getViewport(0, 0, viewWidth, viewHeight);
            VkRect2D scissor;
            scissor.offset = {0, 0};
            scissor.extent.width = viewWidth;
            scissor.extent.height = viewHeight;

            vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irrPipeline);
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irrPipelineLayout, 0, 1, &irrDescriptorSet, 0, nullptr);
            vkCmdPushConstants(commandBuffer, irrPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);
            m_skyboxLoader.bindBuffers(commandBuffer);
            m_skyboxLoader.draw(commandBuffer);
            vkCmdEndRenderPass(commandBuffer);

            //copy to cube
            Tools::setImageLayout(commandBuffer, frameImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

            VkImageSubresourceRange cubeSubresourceRange = {};
            cubeSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cubeSubresourceRange.baseArrayLayer = face;
            cubeSubresourceRange.layerCount = 1;
            cubeSubresourceRange.baseMipLevel = level;
            cubeSubresourceRange.levelCount = 1;

            Tools::setImageLayout(commandBuffer, m_irrImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, cubeSubresourceRange);

            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = face;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstSubresource.mipLevel = level;
            copyRegion.srcOffset = {0, 0, 0};
            copyRegion.dstOffset = {0, 0, 0};
            copyRegion.extent.width = viewWidth;
            copyRegion.extent.height = viewHeight;
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(commandBuffer, frameImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_irrImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            Tools::setImageLayout(commandBuffer, frameImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

            Tools::setImageLayout(commandBuffer, m_irrImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, cubeSubresourceRange);
        }
    }
    
    Tools::flushCommandBuffer(commandBuffer, m_graphicsQueue, true);
    
    vkDestroyPipeline(m_device, irrPipeline, nullptr);
    vkDestroyFramebuffer(m_device, irrFrameBuffer, nullptr);
    vkDestroyRenderPass(m_device, irrRenderPass, nullptr);
    vkDestroyDescriptorPool(m_device, irrDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, irrDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_device, irrPipelineLayout, nullptr);
    vkFreeMemory(m_device, frameMemory, nullptr);
    vkDestroyImage(m_device, frameImage, nullptr);
    vkDestroyImageView(m_device, frameImageView, nullptr);
}

void PbrIbl::generatePrefilteredCube()
{}

void PbrIbl::selectPbrMaterial()
{
    m_pbrMaterial = PbrMaterial("Gold", glm::vec3(1.0f, 0.765557f, 0.336057f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Copper", glm::vec3(0.955008f, 0.637427f, 0.538163f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Chromium", glm::vec3(0.549585f, 0.556114f, 0.554256f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Nickel", glm::vec3(0.659777f, 0.608679f, 0.525649f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Titanium", glm::vec3(0.541931f, 0.496791f, 0.449419f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Cobalt", glm::vec3(0.662124f, 0.654864f, 0.633732f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Platinum", glm::vec3(0.672411f, 0.637331f, 0.585456f), 0.1f, 1.0f);
//    // Testing materials
//    m_pbrMaterial = PbrMaterial("White", glm::vec3(1.0f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Red", glm::vec3(1.0f, 0.0f, 0.0f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.1f, 1.0f);
//    m_pbrMaterial = PbrMaterial("Black", glm::vec3(0.0f), 0.1f, 1.0f);
}

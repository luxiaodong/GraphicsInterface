
#include "shadowmappingcascade.h"

ShadowMappingCascade::ShadowMappingCascade(std::string title) : Application(title)
{
}

ShadowMappingCascade::~ShadowMappingCascade()
{}

void ShadowMappingCascade::init()
{
    Application::init();
    
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createOffscreenRenderPass();
    createOffscreenFrameBuffer();
    
    createGraphicsPipeline();
}

void ShadowMappingCascade::initCamera()
{
    m_camera.m_isFirstPersion = true;
    m_camera.setPosition(glm::vec3(-0.12f, 1.14f, -2.25f));
    m_camera.setRotation(glm::vec3(-17.0f, 7.0f, 0.0f));
    m_camera.setPerspective(45.0f, (float)m_width / (float)m_height, m_zNear, m_zFar);
}

void ShadowMappingCascade::setEnabledFeatures()
{
    m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
    m_deviceEnabledFeatures.depthClamp = m_deviceFeatures.depthClamp;
}

void ShadowMappingCascade::clear()
{
    vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_shadowDescriptorSetLayout[0], nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_shadowDescriptorSetLayout[1], nullptr);
    vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
    
    vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    vkDestroyImageView(m_device, m_offscreenDepthImageView, nullptr);
    vkDestroyImage(m_device, m_offscreenDepthImage, nullptr);
    vkFreeMemory(m_device, m_offscreenDepthMemory, nullptr);
    vkDestroySampler(m_device, m_offscreenDepthSampler, nullptr);

    vkFreeMemory(m_device, m_shadowUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_shadowUniformBuffer, nullptr);

    vkDestroyPipeline(m_device, m_debugPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    
    m_terrainLoader.clear();
    m_treeLoader.clear();
    Application::clear();
}

void ShadowMappingCascade::prepareVertex()
{
    m_terrainLoader.loadFromFile(Tools::getModelPath() + "terrain_gridlines.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices|GltfFileLoadFlags::FlipY);
    m_terrainLoader.createVertexAndIndexBuffer();
    m_terrainLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal});
    
    m_treeLoader.loadFromFile(Tools::getModelPath() + "oaktree.gltf", m_graphicsQueue, GltfFileLoadFlags::PreTransformVertices|GltfFileLoadFlags::FlipY);
    m_treeLoader.createVertexAndIndexBuffer();
    m_treeLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Color, VertexComponent::Normal});
}

void ShadowMappingCascade::prepareUniform()
{
    // shadow map
    VkDeviceSize uniformSize = sizeof(ShadowUniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_shadowUniformBuffer, m_shadowUniformMemory);

    updateLightPos();
    updateShadowMapMVP();
    
    // debug or scene
    uniformSize = sizeof(Uniform);
    
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);
    updateUniformMVP();
}

void ShadowMappingCascade::prepareDescriptorSetLayoutAndPipelineLayout()
{
    {
        {
            VkDescriptorSetLayoutBinding binding;
            binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
            VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_shadowDescriptorSetLayout[0]));
        }
        
        {
            VkDescriptorSetLayoutBinding binding;
            binding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
            VkDescriptorSetLayoutCreateInfo createInfo = Tools::getDescriptorSetLayoutCreateInfo(&binding, 1);
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_shadowDescriptorSetLayout[1]));
        }
        
        createPipelineLayout(m_shadowDescriptorSetLayout, 2, m_shadowPipelineLayout);
    }
    
    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings;
        bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
        bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        createDescriptorSetLayout(bindings.data(), 2);
        createPipelineLayout();
    }
}

void ShadowMappingCascade::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), SHADOW_MAP_CASCADE_COUNT + 1);
    
    {
        createDescriptorSet(&m_shadowDescriptorSetLayout[0], 1, m_shadowDescriptorSet);

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfo.buffer = m_shadowUniformBuffer;

        std::array<VkWriteDescriptorSet, 1> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_shadowDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        
        for(int i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i)
        {
            createDescriptorSet(&m_shadowDescriptorSetLayout[1], 1, m_cascades[i].descriptorSet);
            
            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            imageInfo.imageView = m_cascades[i].view;
            imageInfo.sampler = m_offscreenDepthSampler;
            
            std::array<VkWriteDescriptorSet, 1> writes = {};
            writes[0] = Tools::getWriteDescriptorSet(m_cascades[i].descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo);
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offscreenDepthImageView;
        imageInfo.sampler = m_offscreenDepthSampler;
        
        std::array<VkWriteDescriptorSet, 2> writes = {};
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo);
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void ShadowMappingCascade::createGraphicsPipeline()
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
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };

    VkPipelineDynamicStateCreateInfo dynamic = Tools::getPipelineDynamicStateCreateInfo(dynamicStates);
    VkPipelineRasterizationStateCreateInfo rasterization = Tools::getPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    std::array<VkPipelineColorBlendAttachmentState, 1> colorBlendAttachments;
    colorBlendAttachments[0] = Tools::getPipelineColorBlendAttachmentState(VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo( static_cast<uint32_t>(colorBlendAttachments.size()), colorBlendAttachments.data());

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = 0;
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
//    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(m_pipelineLayout, m_renderPass);
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();

//    createInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

    createInfo.pVertexInputState = m_terrainLoader.getPipelineVertexInputState();
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    createInfo.layout = m_shadowPipelineLayout;
    createInfo.renderPass = m_offscreenRenderPass;
    rasterization.depthBiasEnable = VK_TRUE;
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingcascade/depthpass.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingcascade/depthpass.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_shadowPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //debug.
    rasterization.depthBiasEnable = VK_FALSE;
    VkPipelineVertexInputStateCreateInfo emptyInputState = {};
    emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    createInfo.pVertexInputState = &emptyInputState;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    createInfo.layout = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingcascade/debugshadowmap.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmappingcascade/debugshadowmap.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_debugPipeline));
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    //scene.
//    int enablePCF = 0;
//    VkSpecializationMapEntry specializationMapEntry;
//    specializationMapEntry.constantID = 0;
//    specializationMapEntry.size = sizeof(int);
//    specializationMapEntry.offset = 0;
//
//    VkSpecializationInfo specializationInfo = {};
//    specializationInfo.dataSize = sizeof(int);
//    specializationInfo.mapEntryCount = 1;
//    specializationInfo.pMapEntries = &specializationMapEntry;
//    specializationInfo.pData = &enablePCF;
//
//    createInfo.pVertexInputState = m_sceneLoader.getPipelineVertexInputState();
//    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
//    rasterization.depthBiasEnable = VK_FALSE;
//    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmapping/scene.vert.spv");
//    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "shadowmapping/scene.frag.spv");
//    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
//    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
//    shaderStages[1].pSpecializationInfo = &specializationInfo;
//    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline));
//    vkDestroyShaderModule(m_device, vertModule, nullptr);
//    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void ShadowMappingCascade::updateRenderData()
{
    updateLightPos();
    updateShadowMapMVP();
    updateUniformMVP();
}

void ShadowMappingCascade::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
//    m_sceneLoader.bindBuffers(commandBuffer);
//    m_sceneLoader.draw(commandBuffer);
}

// ================= shadow mapping ======================

void ShadowMappingCascade::updateLightPos()
{
    static float passTime = 0.001;
    float angle = glm::radians(passTime * 360.0f);
    float radius = 20.0f;
    m_lightPos = glm::vec3(cos(angle) * radius, -radius, sin(angle) * radius);
    passTime += 0.001f;
}

/*
    Calculate frustum split depths and matrices for the shadow map cascades
    Based on https://johanmedestrom.wordpress.com/2016/03/18/opengl-cascaded-shadow-maps/
*/
void ShadowMappingCascade::updateCascades()
{
    float cascadeSplitLambda = 0.95f;
    float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

    float nearClip = m_camera.getNearClip();
    float farClip = m_camera.getFarClip();
    float clipRange = farClip - nearClip;

    float minZ = nearClip;
    float maxZ = nearClip + clipRange;

    float range = maxZ - minZ;
    float ratio = maxZ / minZ;

    // Calculate split depths based on view camera frustum
    // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    // In Unity, cascadeSplits is config
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
    {
        float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
        float log = minZ * std::pow(ratio, p);
        float uniform = minZ + range * p;
        float d = cascadeSplitLambda * (log - uniform) + uniform;
        cascadeSplits[i] = (d - nearClip) / clipRange;
    }

    // Calculate orthographic projection matrix for each cascade
    float lastSplitDist = 0.0;
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
    {
        float splitDist = cascadeSplits[i];
        
        glm::vec3 frustumCorners[8] =
        {
            glm::vec3(-1.0f,  1.0f, -1.0f),
            glm::vec3( 1.0f,  1.0f, -1.0f),
            glm::vec3( 1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f,  1.0f,  1.0f),
            glm::vec3( 1.0f,  1.0f,  1.0f),
            glm::vec3( 1.0f, -1.0f,  1.0f),
            glm::vec3(-1.0f, -1.0f,  1.0f),
        };

        // Project frustum corners into world space
        glm::mat4 invCam = glm::inverse( m_camera.m_projMat * m_camera.m_viewMat );
        for (uint32_t i = 0; i < 8; i++)
        {
            glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
            frustumCorners[i] = invCorner / invCorner.w;
        }
        
        // frustumCorners in view space. ndc是非线形的, view是线形的.

        for (uint32_t i = 0; i < 4; i++)
        {
            glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
            frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
            frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
        }
        
        // 插值 frustumCorners 四个顶点.

        // Get frustum center
        glm::vec3 frustumCenter = glm::vec3(0.0f);
        for (uint32_t i = 0; i < 8; i++) {
            frustumCenter += frustumCorners[i];
        }
        frustumCenter /= 8.0f;
        
        // frustumCenter 为 视锥体的中心点

        float radius = 0.0f;
        for (uint32_t i = 0; i < 8; i++) {
            float distance = glm::length(frustumCorners[i] - frustumCenter);
            radius = glm::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;
        
        // 这里没有解方程算半径, 直接用半径估算.

        glm::vec3 maxExtents = glm::vec3(radius);
        glm::vec3 minExtents = -maxExtents;

        glm::vec3 lightDir = normalize(-m_lightPos);
        glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

        // Store split distance and matrix in cascade
        m_cascades[i].splitDepth = (nearClip + splitDist * clipRange) * -1.0f; // 这里为什么要乘 -1
        m_cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

        lastSplitDist = cascadeSplits[i];
    }
}


void ShadowMappingCascade::updateShadowMapMVP()
{
    // Matrix from light's point of view
//    glm::mat4 depthProjectionMatrix = glm::perspective(glm::radians(m_lightFOV), 1.0f, m_zNear, m_zFar);
//    glm::mat4 depthViewMatrix = glm::lookAt(m_lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
//    glm::mat4 depthModelMatrix = glm::mat4(1.0f);

//    m_shadowUniformMvp.shadowMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
//    Tools::mapMemory(m_shadowUniformMemory, sizeof(ShadowUniform), &m_shadowUniformMvp);
}

void ShadowMappingCascade::updateUniformMVP()
{
    Uniform mvp = {};
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.modelMatrix = glm::mat4(1.0f);
//    mvp.shadowMapMvp = m_shadowUniformMvp.shadowMVP;
    mvp.lightPos = glm::vec4(m_lightPos, 1.0f);
    mvp.zNear = m_zNear;
    mvp.zFar = m_zFar;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
}

void ShadowMappingCascade::createOtherBuffer()
{
    // depth
    Tools::createImageAndMemoryThenBind(m_offscreenDepthFormat, m_shadowMapWidth, m_shadowMapHeight, 1, SHADOW_MAP_CASCADE_COUNT,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_offscreenDepthImage, m_offscreenDepthMemory);
    
    Tools::createImageView(m_offscreenDepthImage, m_offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, SHADOW_MAP_CASCADE_COUNT, m_offscreenDepthImageView, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_offscreenDepthSampler);
}

void ShadowMappingCascade::createOffscreenRenderPass()
{
    VkAttachmentDescription attachmentDescription;
    attachmentDescription = Tools::getAttachmentDescription(m_offscreenDepthFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
 
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 0;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 0;
    subpassDescription.pColorAttachments = nullptr;
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
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &attachmentDescription;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    
    VK_CHECK_RESULT( vkCreateRenderPass(m_device, &createInfo, nullptr, &m_offscreenRenderPass) );
}

void ShadowMappingCascade::createOffscreenFrameBuffer()
{
    for(int i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i)
    {
        Tools::createImageView(m_offscreenDepthImage, m_offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, SHADOW_MAP_CASCADE_COUNT, m_cascades[i].view, VK_IMAGE_VIEW_TYPE_2D_ARRAY, i);
        
        VkFramebufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = m_offscreenRenderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &m_cascades[i].view;
        createInfo.width = m_shadowMapWidth;
        createInfo.height = m_shadowMapHeight;
        createInfo.layers = 1;
        
        VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_cascades[i].frameBuffer));
    }
}

void ShadowMappingCascade::createOtherRenderPass(const VkCommandBuffer& commandBuffer)
{
    for(uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; ++i)
    {
        std::array<VkClearValue, 1> clearValues = {};
        clearValues[0].depthStencil = {1.0f, 0};
        
        VkRenderPassBeginInfo passBeginInfo = {};
        passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passBeginInfo.renderPass = m_offscreenRenderPass;
        passBeginInfo.framebuffer = m_cascades[i].frameBuffer;
        passBeginInfo.renderArea.offset = {0, 0};
        passBeginInfo.renderArea.extent.width = m_shadowMapWidth;
        passBeginInfo.renderArea.extent.height = m_shadowMapHeight;
        passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        passBeginInfo.pClearValues = clearValues.data();
        
        vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        VkViewport viewport = Tools::getViewport(0, 0, m_shadowMapWidth, m_shadowMapHeight);
        VkRect2D scissor;
        scissor.offset = {0, 0};
        scissor.extent.width = m_shadowMapWidth;
        scissor.extent.height = m_shadowMapHeight;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        // vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelineLayout, 0, 1, &m_shadowDescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelineLayout, 1, 1, &m_cascades[i].descriptorSet, 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);
        
        // Floor
        PushConstantData pushConstBlock = { glm::vec4(0.0f), i};
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &pushConstBlock);
        m_terrainLoader.bindBuffers(commandBuffer);
        m_terrainLoader.draw(commandBuffer);
        
        // Trees
        const std::vector<glm::vec3> positions =
        {
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.25f, 0.25f, 1.25f),
            glm::vec3(-1.25f, -0.2f, 1.25f),
            glm::vec3(1.25f, 0.1f, -1.25f),
            glm::vec3(-1.25f, -0.25f, -1.25f),
        };
        
        for (auto position : positions)
        {
            pushConstBlock.position = glm::vec4(position, i);
            vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &pushConstBlock);
            m_treeLoader.bindBuffers(commandBuffer);
            m_treeLoader.draw(commandBuffer);
        }
        
        vkCmdEndRenderPass(commandBuffer);
    }
}


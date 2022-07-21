
#include "particlefire.h"

ParticleFire::ParticleFire(std::string title) : Application(title)
{
}

ParticleFire::~ParticleFire()
{}

void ParticleFire::init()
{
    Application::init();
    
    prepareParticle();
    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    
    createGraphicsPipeline();
}

void ParticleFire::initCamera()
{
    m_camera.setPosition(glm::vec3(0.0f, 0.0f, -75.0f));
    m_camera.setRotation(glm::vec3(-15.0f, 45.0f, 0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 1.0f, 256.0f);
}

void ParticleFire::clear()
{
    vkDestroyPipeline(m_device, m_particlePipeline, nullptr);
    vkFreeMemory(m_device, m_particleUniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_particleUniformBuffer, nullptr);
    
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkFreeMemory(m_device, m_uniformMemory, nullptr);
    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
    vkFreeMemory(m_device, m_particleMemory, nullptr);
    vkDestroyBuffer(m_device, m_particleBuffer, nullptr);

    m_pFire->clear();
    delete m_pFire;
    m_pSmoke->clear();
    delete m_pSmoke;
    m_pFloorColor->clear();
    delete m_pFloorColor;
    m_pFloorNormal->clear();
    delete m_pFloorNormal;
    m_environmentLoader.clear();
    Application::clear();
}

void ParticleFire::prepareVertex()
{
    m_environmentLoader.loadFromFile(Tools::getModelPath() + "fireplace.gltf", m_graphicsQueue);
    m_environmentLoader.createVertexAndIndexBuffer();
    m_environmentLoader.setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::UV, VertexComponent::Normal, VertexComponent::Tangent});

    m_pFloorColor = Texture::loadTextrue2D(Tools::getTexturePath() + "fireplace_colormap_rgba.ktx", m_graphicsQueue);
    m_pFloorNormal = Texture::loadTextrue2D(Tools::getTexturePath() + "fireplace_normalmap_rgba.ktx", m_graphicsQueue);
    
    m_pFire = Texture::loadTextrue2D(Tools::getTexturePath() + "particle_fire.ktx", m_graphicsQueue);
    m_pSmoke = Texture::loadTextrue2D(Tools::getTexturePath() + "particle_smoke.ktx", m_graphicsQueue);
    
// 采样器.
//    VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK
//    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
//            samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
//            samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
}

void ParticleFire::prepareUniform()
{
    VkDeviceSize uniformSize = sizeof(Uniform);
    Tools::createBufferAndMemoryThenBind(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_uniformBuffer, m_uniformMemory);

    Uniform mvp = {};
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.normalMatrix = glm::inverse( glm::transpose(m_camera.m_viewMat));
//    mvp.normalMatrix = glm::transpose( glm::inverse(m_camera.m_viewMat));
    mvp.lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    Tools::mapMemory(m_uniformMemory, uniformSize, &mvp);
    
    // pass 1
    VkDeviceSize totalSize = sizeof(ParticleUniform);
    Tools::createBufferAndMemoryThenBind(totalSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_particleUniformBuffer, m_particleUniformMemory);
    
    ParticleUniform part = {};
    part.projection = m_camera.m_projMat;
    part.modelView = m_camera.m_viewMat;
    part.viewportDim = glm::vec2((float)m_swapchainExtent.width, (float)m_swapchainExtent.height);
    part.pointSize = 10.0f;
    
    Tools::mapMemory(m_particleUniformMemory, totalSize, &part);
}

void ParticleFire::prepareDescriptorSetLayoutAndPipelineLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> bindings;
    bindings[0] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    bindings[1] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
    bindings[2] = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2);
    createDescriptorSetLayout(bindings.data(), static_cast<uint32_t>(bindings.size()));
    createPipelineLayout();
}

void ParticleFire::prepareDescriptorSetAndWrite()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 4;
    
    createDescriptorPool(poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), 2);
    
    {
        createDescriptorSet(m_particleDescriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(ParticleUniform);
        bufferInfo.buffer = m_particleUniformBuffer;
        
        VkDescriptorImageInfo imageInfo1 = m_pSmoke->getDescriptorImageInfo();
        VkDescriptorImageInfo imageInfo2 = m_pFire->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 3> writes;
        writes[0] = Tools::getWriteDescriptorSet(m_particleDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_particleDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo1);
        writes[2] = Tools::getWriteDescriptorSet(m_particleDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo2);
        vkUpdateDescriptorSets(m_device, writes.size(), writes.data(), 0, nullptr);
    }
    
    {
        createDescriptorSet(m_descriptorSet);
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Uniform);
        bufferInfo.buffer = m_uniformBuffer;
        
        VkDescriptorImageInfo imageInfo1 = m_pFloorColor->getDescriptorImageInfo();
        VkDescriptorImageInfo imageInfo2 = m_pFloorNormal->getDescriptorImageInfo();
        
        std::array<VkWriteDescriptorSet, 3> writes;
        writes[0] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo);
        writes[1] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo1);
        writes[2] = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageInfo2);
        vkUpdateDescriptorSets(m_device, writes.size(), writes.data(), 0, nullptr);
    }
}

void ParticleFire::createGraphicsPipeline()
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
    
    createInfo.pVertexInputState = m_environmentLoader.getPipelineVertexInputState();;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamic;
    createInfo.subpass = 0;
    
    VkShaderModule vertModule = Tools::createShaderModule( Tools::getShaderPath() + "particlefire/normalmap.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule( Tools::getShaderPath() + "particlefire/normalmap.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
    
    // particle file
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    VkVertexInputBindingDescription vertexInputBinding =  Tools::getVertexInputBindingDescription(0, sizeof(Particle));
    
    std::array<VkVertexInputAttributeDescription, 6> vertexInputAttributes;
    vertexInputAttributes[0] = Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, pos));
    vertexInputAttributes[1] = Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, color));
    vertexInputAttributes[2] = Tools::getVertexInputAttributeDescription(0, 2, VK_FORMAT_R32_SFLOAT, offsetof(Particle, alpha));
    vertexInputAttributes[3] = Tools::getVertexInputAttributeDescription(0, 3, VK_FORMAT_R32_SFLOAT, offsetof(Particle, size));
    vertexInputAttributes[4] = Tools::getVertexInputAttributeDescription(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(Particle, rotation));
    vertexInputAttributes[5] = Tools::getVertexInputAttributeDescription(0, 5, VK_FORMAT_R32_SINT, offsetof(Particle, type));
    
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInput.pVertexAttributeDescriptions = vertexInputAttributes.data();
    
    createInfo.pVertexInputState = &vertexInput;
    depthStencil.depthWriteEnable = VK_FALSE;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment2 = Tools::getPipelineColorBlendAttachmentState(VK_TRUE);
    colorBlendAttachment2.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    
    VkPipelineColorBlendStateCreateInfo colorBlend2 = Tools::getPipelineColorBlendStateCreateInfo(1, &colorBlendAttachment2);
    createInfo.pColorBlendState = &colorBlend2;
    
    vertModule = Tools::createShaderModule( Tools::getShaderPath() + "particlefire/particle.vert.spv");
    fragModule = Tools::createShaderModule( Tools::getShaderPath() + "particlefire/particle.frag.spv");
    shaderStages[0] = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);

    if( vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_particlePipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void ParticleFire::updateRenderData()
{
    static int frame = 0;
    frame++;
    
    Uniform mvp = {};
    mvp.viewMatrix = m_camera.m_viewMat;
    mvp.projectionMatrix = m_camera.m_projMat;
    mvp.normalMatrix = glm::inverse( glm::transpose(m_camera.m_viewMat));
    mvp.lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    mvp.lightPos.x = sin(frame * 0.01f * float(M_PI)) * 1.5f;
    mvp.lightPos.z = cos(frame * 0.01f * float(M_PI)) * 1.5f;
    Tools::mapMemory(m_uniformMemory, sizeof(Uniform), &mvp);
    
    updateParticles();
}

void ParticleFire::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    VkViewport viewport = Tools::getViewport(0, 0, m_swapchainExtent.width, m_swapchainExtent.height);
    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    m_environmentLoader.bindBuffers(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    m_environmentLoader.draw(commandBuffer);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_particlePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_particleDescriptorSet, 0, nullptr);
    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_particleBuffer, offsets);
    vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);
}


void ParticleFire::prepareParticle()
{
    m_particles.resize(PARTICLE_COUNT);
    for (auto& particle : m_particles)
    {
        initParticle(&particle);
        particle.alpha = 1.0f - (abs(particle.pos.y) / (-8.0 * 2.0f));
    }
    
    VkDeviceSize totalSize = PARTICLE_COUNT * sizeof(Particle);
    
    Tools::createBufferAndMemoryThenBind(totalSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_particleBuffer, m_particleMemory);
    
    Tools::mapMemory(m_particleMemory, totalSize, m_particles.data());
}

void ParticleFire::initParticle(Particle* particle, glm::vec3 emitterPos)
{
    particle->vel = glm::vec4(0.0f, m_minVel.y + (m_maxVel.y - m_minVel.y) * Tools::random01(), 0.0f, 0.0f);
    particle->alpha = 0.75f * Tools::random01();
    particle->size = 1.0f + 0.5f * Tools::random01();
    particle->color = glm::vec4(1.0f);
    particle->type = 0;
    particle->rotation = 2.0f * float(M_PI) * Tools::random01();
    particle->rotationSpeed = 2.0f*Tools::random01() - 2.0f * Tools::random01();

    // Get random sphere point
    float theta = 2.0f * float(M_PI) * Tools::random01();
    float phi =  - float(M_PI)/2.0f + float(M_PI) * Tools::random01();
    float r = 8.0f *  Tools::random01(); //rnd(FLAME_RADIUS);

    particle->pos.x = r * cos(theta) * cos(phi);
    particle->pos.y = r * sin(phi);
    particle->pos.z = r * sin(theta) * cos(phi);

    particle->pos += glm::vec4(emitterPos, 0.0f);
}

void ParticleFire::transitionParticle(Particle *particle)
{
    if(particle->type == 0)
    {
        if ( Tools::random01() < 0.05f)
        {
            particle->alpha = 0.0f;
            particle->color = glm::vec4(0.25f + 0.25f*Tools::random01());
            particle->pos.x *= 0.5f;
            particle->pos.z *= 0.5f;
            particle->vel.x = Tools::random01() - Tools::random01();
            particle->vel.y = m_minVel.y * 2.0f + (m_maxVel.y - m_minVel.y) * Tools::random01();
            particle->vel.z = Tools::random01() - Tools::random01();
            particle->vel.w = 0.0f;
            particle->size = 1.0f + 0.5f * Tools::random01();
            particle->rotationSpeed = Tools::random01() - Tools::random01();
            particle->type = 1.0f;
        }
        else
        {
            initParticle(particle);
        }
    }
    else
    {
        initParticle(particle);
    }
}

void ParticleFire::updateParticles()
{
    float frameTimer = 0.04;
    float particleTimer = frameTimer * 0.45f;
    for (auto& particle : m_particles)
    {
        if(particle.type == 0)
        {
            particle.pos.y -= particle.vel.y * particleTimer * 3.5f;
            particle.alpha += particleTimer * 2.5f;
            particle.size -= particleTimer * 0.5f;
        }
        else
        {
            particle.pos -= particle.vel * frameTimer * 1.0f;
            particle.alpha += particleTimer * 1.25f;
            particle.size += particleTimer * 0.125f;
            particle.color -= particleTimer * 0.05f;
        }
        
        particle.rotation += particleTimer * particle.rotationSpeed;
        // Transition particle state
        if (particle.alpha > 2.0f)
        {
            transitionParticle(&particle);
        }
    }
    
    VkDeviceSize totalSize = PARTICLE_COUNT * sizeof(Particle);
    Tools::mapMemory(m_particleMemory, totalSize, m_particles.data());
}


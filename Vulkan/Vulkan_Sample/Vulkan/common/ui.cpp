
#include "ui.h"

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <cstring>


Ui::Ui()
{
    // Init ImGui
    ImGui::CreateContext();
    // Color scheme
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    // Dimensions
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;
}

Ui::~Ui()
{
    if(m_vertexBuffer)
    {
        vkFreeMemory(m_device, m_vertexMemory, nullptr);
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    }
    
    if(m_indexBuffer)
    {
        vkFreeMemory(m_device, m_indexMemory, nullptr);
        vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    }
    
    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    
    vkFreeMemory(m_device, m_fontMemory, nullptr);
    vkDestroyImage(m_device, m_fontImage, nullptr);
    vkDestroyImageView(m_device, m_fontImageView, nullptr);
    vkDestroySampler(m_device, m_fontSampler, nullptr);
}

void Ui::prepareResources()
{
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* fontData;
    int texWidth, texHeight;
    
    const std::string fontNamePath = "assets/Roboto-Medium.ttf";
    io.Fonts->AddFontFromFileTTF(fontNamePath.c_str(), 16.0f);
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    VkDeviceSize ttfSize = texWidth * texHeight * 4 * sizeof(char);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    Tools::createBufferAndMemoryThenBind(ttfSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stagingBuffer, stagingMemory);

    Tools::mapMemory(stagingMemory, ttfSize, fontData);
    
    Tools::createImageAndMemoryThenBind(VK_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, 1,
                                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        m_fontImage, m_fontMemory);
    
    Tools::createImageView(m_fontImage, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_ASPECT_COLOR_BIT, 1, m_fontImageView);
    
    VkCommandBuffer cmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    // 设置图片布局再拷贝
    Tools::setImageLayout(cmd, m_fontImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = texWidth;
    region.imageExtent.height = texHeight;
    region.imageExtent.depth = 1;
    
    vkCmdCopyBufferToImage(cmd, stagingBuffer, m_fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    Tools::setImageLayout(cmd, m_fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    
    Tools::flushCommandBuffer(cmd, m_graphicsQueue, true);
    
    vkFreeMemory(m_device, stagingMemory, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    // 创建采样器
    Tools::createTextureSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, m_fontSampler);

    createDescriptorPool();
    createDescriptorSetLayout();

    Tools::allocateDescriptorSets(m_descriptorPool, &m_descriptorSetLayout, 1, m_descriptorSet);

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageView = m_fontImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.sampler = m_fontSampler;
    
    VkWriteDescriptorSet writeDescriptorSet = Tools::getWriteDescriptorSet(m_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo);
    
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
    
    // 字体准备好后可以更新UI了.
    updateUI(60);
}

void Ui::createDescriptorPool()
{
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.poolSizeCount = 1;
    createInfo.pPoolSizes = &poolSize;
    createInfo.maxSets = 2;
    
    if( vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create descriptorPool!");
    }
}

void Ui::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding fontBinding = Tools::getDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    
    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.bindingCount = 1;
    createInfo.pBindings = &fontBinding;

    if ( vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create descriptorSetLayout!");
    }
}

void Ui::createPipelineLayout()
{
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstBlock);
    
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &m_descriptorSetLayout;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;
    
    if( vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipelineLayout!");
    }
}

void Ui::preparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass)
{
    createPipelineLayout();

    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    bindingDescriptions.push_back(Tools::getVertexInputBindingDescription(0, sizeof(ImDrawVert)));
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.push_back(Tools::getVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)));
    attributeDescriptions.push_back(Tools::getVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)));
    attributeDescriptions.push_back(Tools::getVertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)));
    
    VkPipelineVertexInputStateCreateInfo vertexInput = Tools::getPipelineVertexInputStateCreateInfo(bindingDescriptions, attributeDescriptions);
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = Tools::getPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    
    //做占位符用.动态调整
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
    VkPipelineColorBlendAttachmentState olorBlendAttachment = Tools::getPipelineColorBlendAttachmentState(VK_TRUE, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    VkPipelineColorBlendStateCreateInfo colorBlend = Tools::getPipelineColorBlendStateCreateInfo(1, &olorBlendAttachment);
    VkPipelineDepthStencilStateCreateInfo depthStencil = Tools::getPipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
    VkPipelineMultisampleStateCreateInfo multisample = Tools::getPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

    VkShaderModule vertModule = Tools::createShaderModule("assets/base/uioverlay.vert.spv");
    VkShaderModule fragModule = Tools::createShaderModule("assets/base/uioverlay.frag.spv");
    VkPipelineShaderStageCreateInfo vert = Tools::getPipelineShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    VkPipelineShaderStageCreateInfo frag = Tools::getPipelineShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vert);
    shaderStages.push_back(frag);
    VkGraphicsPipelineCreateInfo createInfo = Tools::getGraphicsPipelineCreateInfo(shaderStages, m_pipelineLayout, renderPass);
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
    
    if( vkCreateGraphicsPipelines(m_device, pipelineCache, 1, &createInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS )
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void Ui::updateUI(uint32_t lastFPS)
{
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)m_width, (float)m_height);
    io.DeltaTime = 1;

    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Vulkan Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted(m_title.c_str());
    ImGui::TextUnformatted("Apple Max");
    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);

    ImGui::PushItemWidth(110.0f);
    ImGui::PopItemWidth();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
}

bool Ui::prepareRenderData()
{
    bool rtn = false;
    ImDrawData* imDrawData = ImGui::GetDrawData();

    // Note: Alignment is done inside buffer creation
    VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    // Update buffers only if vertex or index count has been changed compared to current buffer size
    if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
        return rtn;
    }
    
    // 注意: vertexBuffer不能重新创建, vertexMemory可以, 因为vertexBuffer地址已经到了commandBuffer
    //vertex buffer
    if(m_vertexBuffer)
    {
        if(m_vertexBufferSize != vertexBufferSize)
        {
            vkFreeMemory(m_device, m_vertexMemory, nullptr);
            vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }
    }
    
    if(m_vertexBuffer == VK_NULL_HANDLE)
    {
        Tools::createBufferAndMemoryThenBind(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                             m_vertexBuffer, m_vertexMemory);
        m_vertexBufferSize = vertexBufferSize;
        rtn = true;
    }
    
    //index buffer
    if(m_indexBuffer)
    {
        if(m_indexBufferSize != indexBufferSize)
        {
            vkFreeMemory(m_device, m_indexMemory, nullptr);
            vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
        }
    }
    
    if(m_indexBuffer == VK_NULL_HANDLE)
    {
        Tools::createBufferAndMemoryThenBind(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                             m_indexBuffer, m_indexMemory);
        m_indexBufferSize = indexBufferSize;
        rtn = true;
    }
    
    ImDrawVert* vertexDstAddress = nullptr;
    ImDrawIdx* indexDstAddress = nullptr;
    vkMapMemory(m_device, m_vertexMemory, 0, m_vertexBufferSize, 0, (void**)&vertexDstAddress);
    vkMapMemory(m_device, m_indexMemory, 0, m_indexBufferSize, 0, (void**)&indexDstAddress);
    
    for (int n = 0; n < imDrawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vertexDstAddress, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(indexDstAddress, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vertexDstAddress += cmd_list->VtxBuffer.Size;
        indexDstAddress += cmd_list->IdxBuffer.Size;
    }
    
    vkUnmapMemory(m_device, m_vertexMemory);
    vkUnmapMemory(m_device, m_indexMemory);
    
    return rtn;
}

void Ui::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
    ImDrawData* imDrawData = ImGui::GetDrawData();
    if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, NULL);
    
    m_pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    m_pushConstBlock.translate = glm::vec2(-1.0f);
    vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &m_pushConstBlock);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;
    for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
    {
        const ImDrawList* cmd_list = imDrawData->CmdLists[i];
        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
            VkRect2D scissorRect;
            scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
            vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }
        vertexOffset += cmd_list->VtxBuffer.Size;
    }
}

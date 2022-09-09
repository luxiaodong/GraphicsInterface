
#pragma once

#include "tools.h"
#include "stb_font_consolas_24_latin1.inl"

enum TextAlign {Left, Center, Right};

class Text
{
public:
    Text(VkFormat colorformat, VkFormat depthformat, uint32_t framebufferWidth, uint32_t framebufferHeight, std::string vertFilePath, std::string fragFilePath);
    ~Text();

public:
    void init();
    void clear();
    
    void begin();
    void addString(std::string str, float x, float y, float sx = 1.0f, float sy = 1.0f, TextAlign align = TextAlign::Center);
    void test();
    void end();
    void updateCommandBuffers(const VkFramebuffer& frameBuffer);
    void recordRenderCommand(const VkCommandBuffer& commandBuffer);
    
    void prepareResources();
    void createRenderPass();
    void createPipeline();

public:
    std::vector<VkFramebuffer*> m_framebuffers;
    VkFormat m_colorFormat;
    VkFormat m_depthFormat;
    uint32_t m_framebufferWidth;
    uint32_t m_framebufferHeight;
    std::string m_vertFilePath;
    std::string m_fragFilePath;
    
private:
    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffer;
    
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    
    VkImage m_fontImage;
    VkDeviceMemory m_fontMemory;
    VkImageView m_fontImageView;
    VkSampler m_fontSampler;
    
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;
    VkPipelineLayout m_pipelineLayout;
    
    VkRenderPass m_renderPass;
    VkPipeline m_graphicsPipeline;
    
private:
    stb_fontchar m_stbFontData[STB_FONT_consolas_24_latin1_NUM_CHARS];
    int m_numLetter;
    glm::vec4 *mapped = nullptr;
};

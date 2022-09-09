
#pragma once

#include "common/application.h"
#include "common/gltfModel.h"
#include "common/gltfLoader.h"
#include "common/text.h"

class DistanceFieldFonts : public Application
{
public:
    struct FontVertex
    {
        float pos[3];
        float uv[2];
    };
    
    struct BmpChar {
        uint32_t x, y;
        uint32_t width;
        uint32_t height;
        int32_t xoffset;
        int32_t yoffset;
        int32_t xadvance;
        uint32_t page;
    };
    
    struct UniformVert {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
    };
    
    struct UniformFrag {
        glm::vec4 outlineColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        float outlineWidth = 0.6f;
        float outline = true;
    };

    DistanceFieldFonts(std::string title);
    virtual ~DistanceFieldFonts();

    virtual void init();
    virtual void initCamera();
    virtual void setEnabledFeatures();
    virtual void clear();

    virtual void updateRenderData();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);

protected:
    void prepareVertex();
    void prepareUniform();
    void prepareDescriptorSetLayoutAndPipelineLayout();
    void prepareDescriptorSetAndWrite();
    void createGraphicsPipeline();

    void parseBmpFont();
    void generateText(std::string text);
    
protected:
    VkDescriptorSet m_fontSdfDescriptorSet;
    VkDescriptorSet m_fontBmpDescriptorSet;
    
    VkPipeline m_fontSdfPipeline;
    VkPipeline m_fontBmpPipeline;

    VkBuffer m_uniformVertBuffer;
    VkDeviceMemory m_uniformVertMemory;
    VkBuffer m_uniformFragBuffer;
    VkDeviceMemory m_uniformFragMemory;
    
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    uint32_t m_indexCount;
    
    std::vector<VkVertexInputBindingDescription> m_vertexInputBindDes;
    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttrDes;
    
private:
    Texture* m_pFontSdf;
    Texture* m_pFontBmp;
    
    std::array<BmpChar, 255> m_bmpChars;
};

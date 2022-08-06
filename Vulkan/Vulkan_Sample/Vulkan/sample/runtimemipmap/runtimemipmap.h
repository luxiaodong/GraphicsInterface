
#pragma once

#include "common/application.h"
#include "common/texture.h"
#include "common/gltfLoader.h"

class RuntimeMipmap : public Application
{
public:
    struct Uniform {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 viewPos;
        float lodBias = 0.0f;
        int32_t samplerIndex = 2;
    };
    
    RuntimeMipmap(std::string title);
    virtual ~RuntimeMipmap();
    
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
    
    void generateMipmaps();

protected:
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;

    VkBuffer m_uniformBuffer;
    VkDeviceMemory m_uniformMemory;
    
    // m_pTexture 存放原始图片, image存放mipmap图片
    VkImage         m_image;
    VkDeviceMemory  m_imageMemory;
    VkImageView     m_imageView;
    uint32_t m_mipLevels;
    VkSampler m_sampler0; // no mipmap;
    VkSampler m_sampler1; // mipmap,bilinear
    VkSampler m_sampler2; // mipmap,anisotropic
    
private:
    GltfLoader m_sceneLoader;
    Texture* m_pTexture = nullptr;
};

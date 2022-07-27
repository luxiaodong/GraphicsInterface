
#pragma once

#include "tools.h"
#include "texture.h"

class Material
{
public:
    Material();
    ~Material();
    
    enum AlphaMode {OPAQUE, MASK, BLEND};
    
public:
    void clear();
//    void createMaterialBuffer();

public:
    AlphaMode m_alphaMode = AlphaMode::OPAQUE;
    float m_alphaCutoff = 1.0f;
    float m_metallic = 1.0f;
    float m_roughness = 1.0f;
    glm::vec4 m_baseColor = glm::vec4(1.0f);
    Texture* m_pBaseColorTexture = nullptr;
    Texture* m_pNormalTexture = nullptr;
    
public:
    VkDescriptorSet m_descriptorSet;
    VkPipeline m_graphicsPipeline;
    bool m_isNeedVkBuffer = false;
};


#pragma once

#include "tools.h"

class Material
{
public:
    Material();
    ~Material();

public:
    float m_alphaCutoff = 1.0f;
    float m_metallic = 1.0f;
    float m_roughness = 1.0f;
    glm::vec4 m_baseColor = glm::vec4(1.0f);
};

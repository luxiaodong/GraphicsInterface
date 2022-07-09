
#pragma once

#include "tools.h"

class Mesh;

class GltfNode
{
public:
    GltfNode();
    ~GltfNode();
    
    glm::mat4 localMatrix();
    glm::mat4 worldMatrix();
    
public:
    GltfNode* m_parent;
    uint32_t m_indexAtScene;
    std::string m_name;
    Mesh* m_mesh = nullptr;
    uint32_t m_skinIndex = -1;

    glm::vec3 m_translation = glm::vec3(0.0f);
    glm::quat m_rotation = glm::mat4(1.0f);
    glm::vec3 m_scale = glm::vec3(1.0f);
    glm::mat4 m_originMat = glm::mat4(1.0f);
    
    std::vector<GltfNode*> m_children;
};

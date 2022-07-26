
#pragma once

#include "tools.h"
#include "primitive.h"

class GltfNode;

class Skin
{
public:
    Skin();
    ~Skin();
    
public:
    std::string m_name;
    GltfNode* m_pRootSkeleton = nullptr;
    std::vector<glm::mat4> m_inverseBindMatrices;
    std::vector<GltfNode*> m_joints;
};

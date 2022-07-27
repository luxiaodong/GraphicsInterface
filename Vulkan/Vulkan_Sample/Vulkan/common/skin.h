
#pragma once

#include "tools.h"
#include "primitive.h"

class GltfNode;

class Skin
{
public:
    Skin();
    ~Skin();
    void clear();
    void createJointMatrixBuffer();
    void update();
    
public:
    std::string m_name;
    GltfNode* m_pRootSkeleton = nullptr;
    std::vector<glm::mat4> m_inverseBindMatrices;
    std::vector<GltfNode*> m_joints;
    
public:
    std::vector<glm::mat4> m_jointMatrices;
    VkBuffer m_jointMatrixBuffer;
    VkDeviceMemory m_jointMatrixMemory;
    VkDescriptorSet m_descriptorSet;
    uint32_t m_totalSize;
    bool m_isNeedVkBuffer = false;
};

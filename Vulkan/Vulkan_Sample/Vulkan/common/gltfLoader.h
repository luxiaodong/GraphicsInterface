
#pragma once

#include "tools.h"
#include "gltfModel.h"

class GltfLoader
{
public:
    GltfLoader();
    ~GltfLoader();
    void clear();
    void loadFromFile(std::string fileName, VkQueue transferQueue);
    
public:
    void bindBuffers(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);
    
public:
    vkglTF::Model* m_pModel = nullptr;
};

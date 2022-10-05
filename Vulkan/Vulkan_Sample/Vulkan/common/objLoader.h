
#pragma once

#include "tools.h"
#include "vertex.h"

class ObjLoader
{
public:
    void loadFromFile(std::string filename);
    void loadFromFile2(std::string filename);
    void loadCustomSphere();
    void clear();
    void createVertexAndIndexBuffer();
    void bindBuffers(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

private:
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    
private:
    std::vector<Vertex> m_vertexData;
    std::vector<uint32_t> m_indexData;
};

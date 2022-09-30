
#pragma once

#include "tools.h"
#include "vertex.h"

struct MeshVertexDataDefinition
{
    float x, y, z;    // position
    float nx, ny, nz; // normal
    float tx, ty, tz; // tangent
    float u, v;       // UV coordinates
    
    bool operator==(const MeshVertexDataDefinition& other) const
    {
        return  x == other.x && y == other.y && z == other.z &&
                nx == other.nx && ny == other.ny && nz == other.nz &&
                tx == other.tx && ty == other.ty && tz == other.tz &&
                u == other.u && v == other.v;
    }
};

class ObjLoader
{
public:
    void loadFromFile(std::string filename);
    void loadFromFile2(std::string filename);
    void loadFromFile3(std::string filename);
    void loadFromFile4(std::string filename);
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


#pragma once

#include "tools.h"
#include "gltfModel.h"
#include "gltfNode.h"

#include "vertex.h"
#include "primitive.h"
#include "texture.h"
#include "mesh.h"

//#define USE_BUILDIN_LOAD_GLTF 1

enum GltfFileLoadFlags {
    None = 0x00000000,
    PreTransformVertices = 0x00000001,
    PreMultiplyVertexColors = 0x00000002,
    FlipY = 0x00000004,
    DontLoadImages = 0x00000008
};

class GltfLoader
{
public:
    GltfLoader();
    ~GltfLoader();
    void clear();
    void loadFromFile(std::string fileName, VkQueue transferQueue, uint32_t loadFlags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY | GltfFileLoadFlags::DontLoadImages);

public:
    VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState();
    void bindBuffers(VkCommandBuffer commandBuffer);
    void createVertexAndIndexBuffer();
    void setVertexBindingAndAttributeDescription(const std::vector<VertexComponent> components);
    void draw(VkCommandBuffer commandBuffer);

private:
    void load(std::string fileName);
    void loadNodes();
    void loadSingleNode(GltfNode* parent, const tinygltf::Node &node, uint32_t indexAtScene);

    void loadMaterials();
    void loadMesh(Mesh* newMesh, const tinygltf::Mesh &mesh);

    void loadImages();
    void loadAnimations();
    void loadSkins();

private:
    void drawNode(VkCommandBuffer commandBuffer, GltfNode* node);
    void drawNode(VkCommandBuffer commandBuffer, GltfNode* node, const VkPipelineLayout& pipelineLayout);

private:
    uint32_t m_loadFlags;
    
    tinygltf::Model m_gltfModel;
    
    std::vector<Material*> m_materials;
    
    //两种结点组织方式
    std::vector<GltfNode*> m_treeNodes;
    std::vector<GltfNode*> m_linearNodes;
    
    //顶点数据
    std::vector<Vertex> m_vertexData;
    std::vector<uint32_t> m_indexData;

private:
    VkQueue m_graphicsQueue;
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    
public:
#ifdef USE_BUILDIN_LOAD_GLTF
    vkglTF::Model* m_pModel = nullptr;
#endif
};


#pragma once

#include "tools.h"
#include "gltfModel.h"
#include "gltfNode.h"

#include "vertex.h"
#include "primitive.h"
#include "texture.h"
#include "mesh.h"
#include "skin.h"
#include "animation.h"

//#define USE_BUILDIN_LOAD_GLTF 1

enum GltfFileLoadFlags {
    None = 0x00000000,
    PreTransformVertices = 0x00000001,
    PreMultiplyVertexColors = 0x00000002,
    FlipY = 0x00000004,
    DontLoadImages = 0x00000008
};

enum GltfDescriptorBindingFlags
{
    ImageBaseColor = 0x00000001,
    ImageNormalMap = 0x00000002
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
    void createDescriptorPoolAndLayout();
    void createJointMatrixBuffer();
    void createMaterialBuffer();
    void setVertexBindingAndAttributeDescription(const std::vector<VertexComponent> components);
    void draw(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer, const VkPipelineLayout& pipelineLayout, int method);

    void updateAnimation(float deltaTime);
    void updateAnimation(uint32_t index, float deltaTime);
    
private:
    void load(std::string fileName);
    void loadNodes();
    void loadSingleNode(GltfNode* parent, const tinygltf::Node &node, uint32_t indexAtScene);

    void loadMaterials();
    void loadMesh(Mesh* newMesh, const tinygltf::Mesh &mesh);

    void loadImages();
    void loadSkins();
    void loadAnimations();
    
    void calculateSceneDimensions();

private:
    void drawNode(VkCommandBuffer commandBuffer, GltfNode* node, const VkPipelineLayout& pipelineLayout, int method);
    GltfNode* findNode(GltfNode *parent, uint32_t index);
    GltfNode* nodeFromIndex(uint32_t index);
    
private:
    GltfDescriptorBindingFlags m_descriptorBindingFlags;
    uint32_t m_loadFlags;
    std::string m_modelPath;
    
    tinygltf::Model m_gltfModel;

public:
    Texture* m_emptyTexture = nullptr;
    std::vector<Texture*> m_textures;
    std::vector<Material*> m_materials;
    std::vector<Skin*> m_skins;
    std::vector<Animation*> m_animations;
    
    //两种结点组织方式
    std::vector<GltfNode*> m_treeNodes;
    std::vector<GltfNode*> m_linearNodes;
    
    //顶点数据
    std::vector<Vertex> m_vertexData;
    std::vector<uint32_t> m_indexData;
    std::vector<glm::mat4> m_jointMatrices;
    
    glm::vec3 m_min;
    glm::vec3 m_max;
    float m_radius;

private:
    VkQueue m_graphicsQueue;
    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexMemory;
    
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
public:
    static VkDescriptorSetLayout m_uniformDescriptorSetLayout;
    static VkDescriptorSetLayout m_imageDescriptorSetLayout;
    
#ifdef USE_BUILDIN_LOAD_GLTF
    vkglTF::Model* m_pModel = nullptr;
#endif
};

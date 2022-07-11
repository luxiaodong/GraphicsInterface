
#include "gltfLoader.h"

GltfLoader::GltfLoader()
{
#ifdef USE_BUILDIN_LOAD_GLTF
    m_pModel = new vkglTF::Model();
#endif
}

GltfLoader::~GltfLoader()
{}

void GltfLoader::clear()
{
#ifdef USE_BUILDIN_LOAD_GLTF
    if(m_pModel)
    {
        delete m_pModel;
    }
#else
    vkFreeMemory(Tools::m_device, m_vertexMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(Tools::m_device, m_indexMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, m_indexBuffer, nullptr);
#endif
}

void GltfLoader::loadFromFile(std::string fileName, VkQueue transferQueue)
{
#ifdef USE_BUILDIN_LOAD_GLTF
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::DontLoadImages;
    m_pModel->loadFromFile(fileName, transferQueue, glTFLoadingFlags);
#else
    m_graphicsQueue = transferQueue;
    const uint32_t glTFLoadFlags = GltfFileLoadFlags::PreTransformVertices | GltfFileLoadFlags::PreMultiplyVertexColors | GltfFileLoadFlags::FlipY | GltfFileLoadFlags::DontLoadImages;
    load(fileName, glTFLoadFlags);
#endif
}

void GltfLoader::bindBuffers(VkCommandBuffer commandBuffer)
{
#ifdef USE_BUILDIN_LOAD_GLTF
    m_pModel->bindBuffers(commandBuffer);
#else
    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
#endif
}

void GltfLoader::draw(VkCommandBuffer commandBuffer)
{
#ifdef USE_BUILDIN_LOAD_GLTF
    m_pModel->draw(commandBuffer);
#else
    for (auto& node : m_treeNodes)
    {
        drawNode(commandBuffer,node);
    }
#endif
}

// --------------------------------------------------------------------------------------------------------

void GltfLoader::load(std::string fileName, uint32_t fileLoadFlags)
{
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;
    if(gltfContext.LoadASCIIFromFile(&m_gltfModel, &error, &warning, fileName) == false)
    {
        assert(false);
        return ;
    }
    
    this->loadMaterials();
    this->loadNodes();
    
    if ((fileLoadFlags & GltfFileLoadFlags::PreTransformVertices) || (fileLoadFlags & GltfFileLoadFlags::PreMultiplyVertexColors) || (fileLoadFlags & GltfFileLoadFlags::FlipY))
    {
        const bool preTransform = fileLoadFlags & GltfFileLoadFlags::PreTransformVertices;
        const bool preMultiplyColor = fileLoadFlags & GltfFileLoadFlags::PreMultiplyVertexColors;
        const bool flipY = fileLoadFlags & GltfFileLoadFlags::FlipY;
        
        for(GltfNode* node : m_linearNodes)
        {
            if(node->m_mesh)
            {
                const glm::mat4 worldMatrix = node->worldMatrix();
                for (Primitive* primitive : node->m_mesh->m_primitives)
                {
                    for(uint32_t i = 0; i < primitive->m_vertexCount; ++i)
                    {
                        Vertex& vert = m_vertexData[primitive->m_vertexOffset + i];
                        if(preTransform)
                        {
                            vert.m_position = glm::vec3(worldMatrix * glm::vec4(vert.m_position, 1.0f));
                            vert.m_normal = glm::normalize(glm::mat3(worldMatrix) * vert.m_normal);
                        }
                        if(flipY)
                        {
                            vert.m_position.y *= -1.0f;
                            vert.m_normal.y *= -1.0f;
                        }
                        if(preMultiplyColor)
                        {
                            vert.m_color = primitive->m_material->m_baseColor * vert.m_color;
                        }
                    }
                }
            }
        }
    }
    
    createVertexAndIndexBuffer();
    setVertexBindingAndAttributeDescription({VertexComponent::Position, VertexComponent::Normal, VertexComponent::Color});
}

void GltfLoader::loadMaterials()
{
    Material* mat = new Material();
    // add default material;
    m_materials.push_back(mat);
}

void GltfLoader::loadNodes()
{
    const tinygltf::Scene &scene = m_gltfModel.scenes[m_gltfModel.defaultScene > -1 ? m_gltfModel.defaultScene : 0];
    
    //单个场景下索引断言
    assert(scene.nodes[0] == 0);
    assert(scene.nodes[scene.nodes.size() - 1] == scene.nodes.size() - 1);
    
    for (size_t i = 0; i < scene.nodes.size(); i++)
    {
        uint32_t index = scene.nodes[i];
        const tinygltf::Node node = m_gltfModel.nodes[index];
        loadSingleNode(nullptr, node, index);
    }
}

void GltfLoader::loadSingleNode(GltfNode* parent, const tinygltf::Node &node, uint32_t indexAtScene)
{
    GltfNode* newNode = new GltfNode();
    newNode->m_parent = parent;
    newNode->m_indexAtScene = indexAtScene;
    newNode->m_name = node.name;
    newNode->m_skinIndex = node.skin;
    
    if (node.matrix.size() == 16)
    {
        newNode->m_originMat = glm::make_mat4x4(node.matrix.data());
    }
    if (node.scale.size() == 3)
    {
        newNode->m_scale = glm::make_vec3(node.scale.data());
    }
    if (node.rotation.size() == 4)
    {
        newNode->m_rotation = glm::make_quat(node.rotation.data());
    }
    if (node.translation.size() == 3)
    {
        newNode->m_translation = glm::make_vec3(node.translation.data());
    }
    
    if(node.children.size() > 0)
    {
        for (auto i = 0; i < node.children.size(); i++)
        {
            uint32_t subIndex = node.children[i];
            loadSingleNode(newNode, m_gltfModel.nodes[subIndex], subIndex);
        }
    }
    
    if(node.mesh > -1)
    {
        Mesh *newMesh = new Mesh();
        newMesh->m_matrix = newNode->m_originMat;
        const tinygltf::Mesh mesh = m_gltfModel.meshes[node.mesh];
        newMesh->m_name = mesh.name;
        loadMesh(newMesh, mesh);
        newNode->m_mesh = newMesh;
    }
    
    if(parent)
    {
        parent->m_children.push_back(newNode);
    }
    else
    {
        m_treeNodes.push_back(newNode);
    }
    
    m_linearNodes.push_back(newNode);
}

void GltfLoader::loadMesh(Mesh* newMesh, const tinygltf::Mesh &mesh)
{
    for (size_t j = 0; j < mesh.primitives.size(); j++)
    {
        const tinygltf::Primitive &primitive = mesh.primitives[j];
        if (primitive.indices < 0) {
            continue;
        }
        
        //vertex
        assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

        const tinygltf::Accessor &posAccessor = m_gltfModel.accessors[primitive.attributes.find("POSITION")->second];
        const tinygltf::BufferView &posView = m_gltfModel.bufferViews[posAccessor.bufferView];
        const float *bufferPos = reinterpret_cast<const float *>(&(m_gltfModel.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
        glm::vec3 posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
        glm::vec3 posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
        
        const float *bufferNormals = nullptr;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
            const tinygltf::Accessor &normAccessor = m_gltfModel.accessors[primitive.attributes.find("NORMAL")->second];
            const tinygltf::BufferView &normView = m_gltfModel.bufferViews[normAccessor.bufferView];
            bufferNormals = reinterpret_cast<const float *>(&(m_gltfModel.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
        }
        
        const float *bufferTexCoords = nullptr;
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
            const tinygltf::Accessor &uvAccessor = m_gltfModel.accessors[primitive.attributes.find("TEXCOORD_0")->second];
            const tinygltf::BufferView &uvView = m_gltfModel.bufferViews[uvAccessor.bufferView];
            bufferTexCoords = reinterpret_cast<const float *>(&(m_gltfModel.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
        }
        
        uint32_t numColorComponents = 0;
        const float* bufferColors = nullptr;
        if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
        {
            const tinygltf::Accessor& colorAccessor = m_gltfModel.accessors[primitive.attributes.find("COLOR_0")->second];
            const tinygltf::BufferView& colorView = m_gltfModel.bufferViews[colorAccessor.bufferView];
            // Color buffer are either of type vec3 or vec4
            numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
            bufferColors = reinterpret_cast<const float*>(&(m_gltfModel.buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
        }

        uint32_t vertexOffset = static_cast<uint32_t>(m_vertexData.size());
        uint32_t vertexCount = static_cast<uint32_t>(posAccessor.count);
        
        for (size_t i = 0; i < posAccessor.count; i++)
        {
            Vertex vert = {};
            vert.m_position = glm::vec4(glm::make_vec3(&bufferPos[i * 3]), 1.0f);
            vert.m_normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[i * 3]) : glm::vec3(0.0f)));
            vert.m_uv = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[i * 2]) : glm::vec3(0.0f);
            if(numColorComponents > 0)
            {
                if(numColorComponents == 3)
                {
                    vert.m_color = glm::vec4(glm::make_vec3(&bufferColors[i * 3]), 1.0f);
                }
                else if(numColorComponents == 4)
                {
                    vert.m_color = glm::make_vec4(&bufferColors[i * 4]);
                }
            }
            
            m_vertexData.push_back(vert);
        }
        
        //index
        const tinygltf::Accessor &indexAccessor = m_gltfModel.accessors[primitive.indices];
        const tinygltf::BufferView &indexView = m_gltfModel.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer &buffer = m_gltfModel.buffers[indexView.buffer];
        
        uint32_t indexOffset = static_cast<uint32_t>(m_indexData.size());
        uint32_t indexCount = static_cast<uint32_t>(indexAccessor.count);
        
        switch (indexAccessor.componentType)
        {
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
            {
                uint16_t *buf = new uint16_t[indexAccessor.count];
                memcpy(buf, &buffer.data[indexAccessor.byteOffset + indexView.byteOffset], indexAccessor.count * sizeof(uint16_t));
                for (size_t index = 0; index < indexAccessor.count; index++)
                {
                    m_indexData.push_back(buf[index] + vertexOffset);
                }
                delete[] buf;
                break;
            }
                
            default:
                break;
        }
        
        Primitive *newPrimitive = new Primitive();
        newPrimitive->m_vertexOffset = vertexOffset;
        newPrimitive->m_vertexCount = vertexCount;
        newPrimitive->m_indexOffset = indexOffset;
        newPrimitive->m_indexCount = indexCount;
        newPrimitive->m_material = m_materials.back();
        newMesh->m_primitives.push_back(newPrimitive);
    }
}


void GltfLoader::loadImages()
{}

void GltfLoader::loadAnimations()
{}

void GltfLoader::loadSkins()
{}


void GltfLoader::createVertexAndIndexBuffer()
{
    VkBuffer vertexStagingBuffer;
    VkBuffer indexStagingBuffer;
    VkDeviceMemory vertexStagingMemory;
    VkDeviceMemory indexStagingMemory;
    size_t vertexBufferSize = m_vertexData.size() * sizeof(Vertex);
    size_t indexBufferSize = m_indexData.size() * sizeof(uint32_t);
    
    Tools::createBufferAndMemoryThenBind(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingMemory);
    Tools::mapMemory(vertexStagingMemory, vertexBufferSize, m_vertexData.data());
    Tools::createBufferAndMemoryThenBind(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,  m_vertexBuffer, m_vertexMemory);
    
    Tools::createBufferAndMemoryThenBind(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingMemory);
    Tools::mapMemory(indexStagingMemory, indexBufferSize, m_indexData.data());
    Tools::createBufferAndMemoryThenBind(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexMemory);
    
    VkCommandBuffer copyCmd = Tools::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion = {};
    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(copyCmd, vertexStagingBuffer, m_vertexBuffer, 1, &copyRegion);
    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(copyCmd, indexStagingBuffer, m_indexBuffer, 1, &copyRegion);
    Tools::flushCommandBuffer(copyCmd, m_graphicsQueue, true);

    vkDestroyBuffer(Tools::m_device, vertexStagingBuffer, nullptr);
    vkFreeMemory(Tools::m_device, vertexStagingMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, indexStagingBuffer, nullptr);
    vkFreeMemory(Tools::m_device, indexStagingMemory, nullptr);
}

void GltfLoader::setVertexBindingAndAttributeDescription(const std::vector<VertexComponent> components)
{
    Vertex::setVertexInputBindingDescription(0);
    Vertex::setVertexInputAttributeDescription(0,components);
}

VkPipelineVertexInputStateCreateInfo* GltfLoader::getPipelineVertexInputState()
{
#ifdef USE_BUILDIN_LOAD_GLTF
    return vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});
#else
    static VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &Vertex::m_vertexInputBindingDescription;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(Vertex::m_vertexInputAttributeDescriptions.size());
    vertexInput.pVertexAttributeDescriptions = Vertex::m_vertexInputAttributeDescriptions.data();
    return &vertexInput;
#endif
}

void GltfLoader::drawNode(VkCommandBuffer commandBuffer, GltfNode* node)
{
    if (node->m_mesh) {
        for (Primitive* primitive : node->m_mesh->m_primitives)
        {
            bool skip = false;
//            const vkglTF::Material& material = primitive->material;
//            if (renderFlags & RenderFlags::RenderOpaqueNodes) {
//                skip = (material.alphaMode != Material::ALPHAMODE_OPAQUE);
//            }
//            if (renderFlags & RenderFlags::RenderAlphaMaskedNodes) {
//                skip = (material.alphaMode != Material::ALPHAMODE_MASK);
//            }
//            if (renderFlags & RenderFlags::RenderAlphaBlendedNodes) {
//                skip = (material.alphaMode != Material::ALPHAMODE_BLEND);
//            }
            if (!skip)
            {
//                if (renderFlags & RenderFlags::BindImages) {
//                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, bindImageSet, 1, &material.descriptorSet, 0, nullptr);
//                }
                vkCmdDrawIndexed(commandBuffer, primitive->m_indexCount, 1, primitive->m_indexOffset, 0, 0);
            }
        }
    }
    
    for (auto& child : node->m_children) {
        drawNode(commandBuffer, child);
    }
}


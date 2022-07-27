
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
    
    for(Skin* skin : m_skins)
    {
        skin->clear();
        delete skin;
    }
    
    for(Animation* ani : m_animations)
    {
        delete ani;
    }
    
    if(m_emptyTexture)
    {
        m_emptyTexture->clear();
        delete m_emptyTexture;
    }
    
    for(Texture* tex : m_textures)
    {
        tex->clear();
        delete tex;
    }
    
    for(Material* mat : m_materials)
    {
        delete mat;
    }
    
    for(GltfNode* node : m_linearNodes)
    {
        delete node;
    }
    
#endif
}

void GltfLoader::loadFromFile(std::string fileName, VkQueue transferQueue, uint32_t loadFlags)
{
#ifdef USE_BUILDIN_LOAD_GLTF
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::DontLoadImages;
    m_pModel->loadFromFile(fileName, transferQueue, glTFLoadingFlags);
#else
    m_graphicsQueue = transferQueue;
    m_loadFlags = loadFlags;
    load(fileName);
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
    VkPipelineLayout notUseLayout = {};
    draw(commandBuffer, notUseLayout, 0);
}

void GltfLoader::draw(VkCommandBuffer commandBuffer, const VkPipelineLayout& pipelineLayout, int method)
{
#ifdef USE_BUILDIN_LOAD_GLTF
    m_pModel->draw(commandBuffer);
#else
    for (auto& node : m_treeNodes)
    {
        drawNode(commandBuffer, node, pipelineLayout, method);
    }
#endif
}

// --------------------------------------------------------------------------------------------------------

bool callbackImageLoad(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
{
    // KTX files will be handled by our own code
    if (image->uri.find_last_of(".") != std::string::npos) {
        if (image->uri.substr(image->uri.find_last_of(".") + 1) == "ktx") {
            return true;
        }
    }

    return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
}

bool callbackImageLoadEmpty(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
{
    return true;
}

void GltfLoader::load(std::string fileName)
{
    tinygltf::TinyGLTF gltfContext;
    
    if( m_loadFlags & GltfFileLoadFlags::DontLoadImages )
    {
        gltfContext.SetImageLoader(callbackImageLoadEmpty, nullptr);
    }
    else
    {
        gltfContext.SetImageLoader(callbackImageLoad, nullptr);
    }
    
    std::string error, warning;
    if(gltfContext.LoadASCIIFromFile(&m_gltfModel, &error, &warning, fileName) == false)
    {
        assert(false);
        return ;
    }
    
    size_t pos = fileName.find_last_of('/');
    m_modelPath = fileName.substr(0, pos);
    
    if (!(m_loadFlags & GltfFileLoadFlags::DontLoadImages))
    {
        loadImages();
    }

    this->loadMaterials();
    this->loadNodes();
    
    for(GltfNode* node : m_linearNodes)
    {
        if(node->m_mesh)
        {
            node->m_worldMatrix = node->worldMatrix(); //先生成所有的世界矩阵
        }
    }
    
    this->loadSkins();
    this->loadAnimations();
    
    if ((m_loadFlags & GltfFileLoadFlags::PreTransformVertices) || (m_loadFlags & GltfFileLoadFlags::PreMultiplyVertexColors) || (m_loadFlags & GltfFileLoadFlags::FlipY))
    {
        const bool preTransform = m_loadFlags & GltfFileLoadFlags::PreTransformVertices;
        const bool preMultiplyColor = m_loadFlags & GltfFileLoadFlags::PreMultiplyVertexColors;
        const bool flipY = m_loadFlags & GltfFileLoadFlags::FlipY;
        
        for(GltfNode* node : m_linearNodes)
        {
            if(node->m_mesh)
            {
                for (Primitive* primitive : node->m_mesh->m_primitives)
                {
                    for(uint32_t i = 0; i < primitive->m_vertexCount; ++i)
                    {
                        Vertex& vert = m_vertexData[primitive->m_vertexOffset + i];
                        if(preTransform)
                        {
                            vert.m_position = glm::vec3(node->m_worldMatrix * glm::vec4(vert.m_position, 1.0f));
                            vert.m_normal = glm::normalize(glm::mat3(node->m_worldMatrix) * vert.m_normal);
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
}


void GltfLoader::loadImages()
{
    for (tinygltf::Image &image : m_gltfModel.images)
    {
        Texture* tex = Texture::loadTexture2D(image, m_modelPath, m_graphicsQueue);
        m_textures.push_back(tex);
    }
    
    m_emptyTexture = Texture::loadTextureEmpty(m_graphicsQueue);
}

void GltfLoader::loadMaterials()
{
    for (tinygltf::Material &mat : m_gltfModel.materials)
    {
        Material* newMat = new Material();
        if (mat.values.find("roughnessFactor") != mat.values.end())
        {
            newMat->m_roughness = static_cast<float>(mat.values["roughnessFactor"].Factor());
        }
        if (mat.values.find("metallicFactor") != mat.values.end())
        {
            newMat->m_metallic = static_cast<float>(mat.values["metallicFactor"].Factor());
        }
        if (mat.values.find("baseColorFactor") != mat.values.end())
        {
            newMat->m_baseColor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }
        if (mat.values.find("baseColorTexture") != mat.values.end())
        {
            int index = m_gltfModel.textures[mat.values["baseColorTexture"].TextureIndex()].source;
            newMat->m_pBaseColorTexture = m_textures.at(index);
        }
        
        m_materials.push_back(newMat);
    }
    
    // add default material;
    m_materials.push_back(new Material());
}

void GltfLoader::loadNodes()
{
    const tinygltf::Scene &scene = m_gltfModel.scenes[m_gltfModel.defaultScene > -1 ? m_gltfModel.defaultScene : 0];
    
    //单个场景下索引断言
//    assert(scene.nodes[0] == 0);
//    assert(scene.nodes[scene.nodes.size() - 1] == scene.nodes.size() - 1);
    
    for (size_t i = 0; i < scene.nodes.size(); i++)
    {
        uint32_t index = scene.nodes[i];
        const tinygltf::Node node = m_gltfModel.nodes[index];
        loadSingleNode(nullptr, node, index);
    }
}

void GltfLoader::loadSkins()
{
    for (tinygltf::Skin &source : m_gltfModel.skins)
    {
        Skin* newSkin = new Skin();
        newSkin->m_name = source.name;
        
        // Find skeleton root node
        if (source.skeleton > -1)
        {
            newSkin->m_pRootSkeleton = nodeFromIndex(source.skeleton);
        }
        
        // Find joint nodes
        for (int jointIndex : source.joints)
        {
            GltfNode* node = nodeFromIndex(jointIndex);
            if (node)
            {
                newSkin->m_joints.push_back(node);
            }
        }
        
        // Get inverse bind matrices from buffer
        if (source.inverseBindMatrices > -1)
        {
            const tinygltf::Accessor &accessor = m_gltfModel.accessors[source.inverseBindMatrices];
            const tinygltf::BufferView &bufferView = m_gltfModel.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = m_gltfModel.buffers[bufferView.buffer];
            newSkin->m_inverseBindMatrices.resize(accessor.count);
            memcpy(newSkin->m_inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
        }

        newSkin->createJointMatrixBuffer();
        m_skins.push_back(newSkin);
    }
}

void GltfLoader::loadAnimations()
{
    for (tinygltf::Animation &anim : m_gltfModel.animations)
    {
        Animation* newAnimation = new Animation();
        newAnimation->m_name = anim.name;
        
        // Samplers
        for(auto &samp : anim.samplers)
        {
            AnimationSampler sampler = {};
            if(samp.interpolation == "LINEAR")
            {
                sampler.m_samplerType = AnimationSamplerType::Linear;
            }
            if (samp.interpolation == "STEP")
            {
                sampler.m_samplerType = AnimationSamplerType::Step;
            }
            if (samp.interpolation == "CUBICSPLINE")
            {
                sampler.m_samplerType = AnimationSamplerType::CubicSpline;
            }
            
            // Read sampler input time values
            {
                const tinygltf::Accessor &accessor = m_gltfModel.accessors[samp.input];
                const tinygltf::BufferView &bufferView = m_gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = m_gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                float *buf = new float[accessor.count];
                memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(float));
                for (size_t index = 0; index < accessor.count; index++)
                {
                    sampler.m_keyFrames.push_back(buf[index]);
                }
                delete[] buf;
                for (auto input : sampler.m_keyFrames)
                {
                    if (input < newAnimation->m_start)
                    {
                        newAnimation->m_start = input;
                    }
                    
                    if (input > newAnimation->m_end)
                    {
                        newAnimation->m_end = input;
                    }
                }
            }
            
            // Read sampler output T/R/S values
            {
                const tinygltf::Accessor &accessor = m_gltfModel.accessors[samp.output];
                const tinygltf::BufferView &bufferView = m_gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = m_gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                switch (accessor.type)
                {
                    case TINYGLTF_TYPE_VEC3:
                    {
                        glm::vec3 *buf = new glm::vec3[accessor.count];
                        memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::vec3));
                        for (size_t index = 0; index < accessor.count; index++) {
                            sampler.m_values.push_back(glm::vec4(buf[index], 0.0f));
                        }
                        delete[] buf;
                        break;
                    }
                    case TINYGLTF_TYPE_VEC4:
                    {
                        glm::vec4 *buf = new glm::vec4[accessor.count];
                        memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::vec4));
                        for (size_t index = 0; index < accessor.count; index++) {
                            sampler.m_values.push_back(buf[index]);
                        }
                        delete[] buf;
                        break;
                    }
                    default: {
                        std::cout << "unknown type" << std::endl;
                        break;
                    }
                }
            }
            
            newAnimation->m_samplers.push_back(sampler);
        }
        
        // Channels
        for (auto &source: anim.channels)
        {
            AnimationChannel channel = {};
            if(source.target_path == "rotation")
            {
                channel.m_channelType = AnimationChannelType::Rotation;
            }
            if (source.target_path == "translation")
            {
                channel.m_channelType = AnimationChannelType::Translation;
            }
            if (source.target_path == "scale")
            {
                channel.m_channelType = AnimationChannelType::Scale;
            }
            if (source.target_path == "weights") {
                std::cout << "weights not yet supported, skipping channel" << std::endl;
                continue;
            }
            
            channel.m_samplerIndex = source.sampler;
            channel.m_node = nodeFromIndex(source.target_node);
            if (!channel.m_node)
            {
                continue;
            }
            newAnimation->m_channels.push_back(channel);
        }
        
        m_animations.push_back(newAnimation);
    }
}

void GltfLoader::updateAnimation(float time)
{
    for(uint32_t i = 0; i < m_animations.size(); ++i)
    {
        updateAnimation(i, time);
    }
    
    for(uint32_t i = 0; i < m_skins.size(); ++i)
    {
        m_skins.at(i)->update();
    }
}

void GltfLoader::updateAnimation(uint32_t index, float deltaTime)
{
    Animation* newAnimation = m_animations.at(index);
    newAnimation->m_currentTime += deltaTime;
    if(newAnimation->m_currentTime > newAnimation->m_end)
    {
        newAnimation->m_currentTime -= newAnimation->m_end;
    }
    
    if(newAnimation->m_currentTime < newAnimation->m_start || newAnimation->m_currentTime > newAnimation->m_end)
    {
        return ;
    }
    
    for(auto &channel : newAnimation->m_channels)
    {
        AnimationSampler sampler = newAnimation->m_samplers.at(channel.m_samplerIndex);
        for(size_t i = 0; i < sampler.m_keyFrames.size() - 1; ++i)
        {
            float prevFrame = sampler.m_keyFrames.at(i);
            float nextFrame = sampler.m_keyFrames.at(i+1);
            
            if(prevFrame < newAnimation->m_currentTime && newAnimation->m_currentTime < nextFrame)
            {
                float p = (newAnimation->m_currentTime - prevFrame)/(nextFrame - prevFrame);
                
                if(channel.m_channelType == AnimationChannelType::Translation)
                {
                    channel.m_node->m_translation = glm::mix(sampler.m_values[i], sampler.m_values[i+1], p);
                }
                else if(channel.m_channelType == AnimationChannelType::Scale)
                {
                    channel.m_node->m_scale = glm::mix(sampler.m_values[i], sampler.m_values[i+1], p);
                }
                else if(channel.m_channelType == AnimationChannelType::Rotation)
                {
                    glm::quat q1;
                    q1.x = sampler.m_values[i].x;
                    q1.y = sampler.m_values[i].y;
                    q1.z = sampler.m_values[i].z;
                    q1.w = sampler.m_values[i].w;
                    glm::quat q2;
                    q2.x = sampler.m_values[i + 1].x;
                    q2.y = sampler.m_values[i + 1].y;
                    q2.z = sampler.m_values[i + 1].z;
                    q2.w = sampler.m_values[i + 1].w;
                    channel.m_node->m_rotation = glm::normalize(glm::slerp(q1, q2, p));
                }
            }
        }
    }
    
    //更新结点的世界矩阵
    for(auto &channel : newAnimation->m_channels)
    {
        channel.m_node->m_worldMatrix = channel.m_node->worldMatrix();
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
        
        const float* bufferTangents = nullptr;
        if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
        {
            const tinygltf::Accessor &tangentAccessor = m_gltfModel.accessors[primitive.attributes.find("TANGENT")->second];
            const tinygltf::BufferView &tangentView = m_gltfModel.bufferViews[tangentAccessor.bufferView];
            bufferTangents = reinterpret_cast<const float *>(&(m_gltfModel.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
        }
        
        const uint16_t *bufferJoints = nullptr;
        if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
            const tinygltf::Accessor &jointAccessor = m_gltfModel.accessors[primitive.attributes.find("JOINTS_0")->second];
            const tinygltf::BufferView &jointView = m_gltfModel.bufferViews[jointAccessor.bufferView];
            bufferJoints = reinterpret_cast<const uint16_t *>(&(m_gltfModel.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
        }

        const float *bufferWeights = nullptr;
        if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
            const tinygltf::Accessor &uvAccessor = m_gltfModel.accessors[primitive.attributes.find("WEIGHTS_0")->second];
            const tinygltf::BufferView &uvView = m_gltfModel.bufferViews[uvAccessor.bufferView];
            bufferWeights = reinterpret_cast<const float *>(&(m_gltfModel.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
        }
        
        bool hasSkin = (bufferJoints && bufferWeights);

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
            vert.m_tangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[i * 4])) : glm::vec4(0.0f);
            vert.m_jointIndex = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[i * 4])) : glm::vec4(0.0f);
            vert.m_jointWeight = hasSkin ? glm::make_vec4(&bufferWeights[i * 4]) : glm::vec4(0.0f);
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
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
            {
                uint32_t *buf = new uint32_t[indexAccessor.count];
                memcpy(buf, &buffer.data[indexAccessor.byteOffset + indexView.byteOffset], indexAccessor.count * sizeof(uint32_t));
                for (size_t index = 0; index < indexAccessor.count; index++) {
                    m_indexData.push_back(buf[index] + vertexOffset);
                }
                delete[] buf;
                break;
            }
                
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
                
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
            {
                uint8_t *buf = new uint8_t[indexAccessor.count];
                memcpy(buf, &buffer.data[indexAccessor.byteOffset + indexView.byteOffset], indexAccessor.count * sizeof(uint8_t));
                for (size_t index = 0; index < indexAccessor.count; index++)
                {
                    m_indexData.push_back(buf[index] + vertexOffset);
                }
                delete[] buf;
                break;
            }
                
            default:
                throw std::runtime_error("Index component type not support!");
                break;
        }
        
        Primitive *newPrimitive = new Primitive();
        newPrimitive->m_vertexOffset = vertexOffset;
        newPrimitive->m_vertexCount = vertexCount;
        newPrimitive->m_indexOffset = indexOffset;
        newPrimitive->m_indexCount = indexCount;
        if(primitive.material > - 1)
        {
            newPrimitive->m_material = m_materials[primitive.material];
        }
        else
        {
            newPrimitive->m_material = m_materials.back();
        }
        newMesh->m_primitives.push_back(newPrimitive);
    }
}

void GltfLoader::createVertexAndIndexBuffer()
{
#ifdef USE_BUILDIN_LOAD_GLTF
#else
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
#endif
}

void GltfLoader::setVertexBindingAndAttributeDescription(const std::vector<VertexComponent> components)
{
#ifdef USE_BUILDIN_LOAD_GLTF
#else
    Vertex::setVertexInputBindingDescription(0);
    Vertex::setVertexInputAttributeDescription(0,components);
#endif
}

VkPipelineVertexInputStateCreateInfo* GltfLoader::getPipelineVertexInputState()
{
#ifdef USE_BUILDIN_LOAD_GLTF
    return vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});
//    return vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV});
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

void GltfLoader::drawNode(VkCommandBuffer commandBuffer, GltfNode* node, const VkPipelineLayout& pipelineLayout, int method)
{
    const bool preTransform = m_loadFlags & GltfFileLoadFlags::PreTransformVertices;
    const bool dotLoadImage = m_loadFlags & GltfFileLoadFlags::DontLoadImages;
    
    if (node->m_mesh)
    {
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
                
                if(method == 1)
                {
                    if(preTransform == false)
                    {
                        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &node->m_worldMatrix);
                    }
                    
                    if(dotLoadImage == false)
                    {
                        if(primitive->m_material && primitive->m_material->m_pBaseColorTexture)
                        {
                            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &primitive->m_material->m_pBaseColorTexture->m_descriptorSet, 0, nullptr);
                        }
                    }
                }
                else if(method == 2)
                {
                    if(preTransform == false)
                    {
                        glm::mat4 mat(1.0f);
                        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
                    }
                    
                    if(m_skins.size() > 0)
                    {
                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &m_skins.at(0)->m_descriptorSet, 0, nullptr);
                    }
                    
                    if(dotLoadImage == false)
                    {
                        if(primitive->m_material && primitive->m_material->m_pBaseColorTexture)
                        {
                            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &primitive->m_material->m_pBaseColorTexture->m_descriptorSet, 0, nullptr);
                        }
                    }
                }
                
                vkCmdDrawIndexed(commandBuffer, primitive->m_indexCount, 1, primitive->m_indexOffset, 0, 0);
            }
        }
    }
    
    for (auto& child : node->m_children) {
        drawNode(commandBuffer, child, pipelineLayout, method);
    }
}


// helper function

GltfNode* GltfLoader::nodeFromIndex(uint32_t index)
{
    GltfNode* nodeFound = nullptr;
    for (auto &node : m_linearNodes)
    {
        nodeFound = findNode(node, index);
        if (nodeFound) {
            break;
        }
    }
    return nodeFound;
}

GltfNode* GltfLoader::findNode(GltfNode *parent, uint32_t index)
{
    GltfNode* nodeFound = nullptr;
    if (parent->m_indexAtScene == index)
    {
        return parent;
    }
    for (auto& child : parent->m_children)
    {
        nodeFound = findNode(child, index);
        if (nodeFound)
        {
            break;
        }
    }
    return nodeFound;
}



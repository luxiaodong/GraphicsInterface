
#include "objLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.m_position) ^ (hash<glm::vec3>()(vertex.m_color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.m_uv) << 1);
        }
    };
}

void ObjLoader::clear()
{
    vkFreeMemory(Tools::m_device, m_vertexMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, m_vertexBuffer, nullptr);
    vkFreeMemory(Tools::m_device, m_indexMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, m_indexBuffer, nullptr);
}

void ObjLoader::loadFromFile(std::string filename)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    int test_index = 0;
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.m_position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            
            vertex.m_normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2],
            };
            
            vertex.m_uv = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            
            vertex.m_color = {1.0f, 1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(m_vertexData.size());
                m_vertexData.push_back(vertex);
            }

            m_indexData.push_back(uniqueVertices[vertex]);
        }
    }
}

void ObjLoader::createVertexAndIndexBuffer()
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
    Tools::flushCommandBuffer(copyCmd, Tools::m_graphicsQueue, true);

    vkDestroyBuffer(Tools::m_device, vertexStagingBuffer, nullptr);
    vkFreeMemory(Tools::m_device, vertexStagingMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, indexStagingBuffer, nullptr);
    vkFreeMemory(Tools::m_device, indexStagingMemory, nullptr);
}

void ObjLoader::bindBuffers(VkCommandBuffer commandBuffer)
{
    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void ObjLoader::draw(VkCommandBuffer commandBuffer)
{
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m_indexData.size()), 1, 0, 0, 0);
}


void ObjLoader::loadFromFile2(std::string filename)
{
    tinyobj::ObjReader       reader;
    tinyobj::ObjReaderConfig reader_config;
    reader_config.vertex_color = false;
    if (!reader.ParseFromFile(filename, reader_config))
    {
        throw std::runtime_error("parse file error.");
    }

    if (!reader.Warning().empty())
    {
        throw std::runtime_error("parse file error.");
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    m_vertexData.clear();
    
//    std::vector<MeshVertexDataDefinition> mesh_vertices;

    for (size_t s = 0; s < shapes.size(); s++)
    {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            bool with_normal   = true;
            bool with_texcoord = true;

            glm::vec3 vertex[3];
            glm::vec3 normal[3];
            glm::vec2 uv[3];

            // only deals with triangle faces
            if (fv != 3)
            {
                continue;
            }

            // expanding vertex data is not efficient and is for testing purposes only
            for (size_t v = 0; v < fv; v++)
            {
                auto idx = shapes[s].mesh.indices[index_offset + v];
                auto vx  = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                auto vy  = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                auto vz  = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                vertex[v].x = static_cast<float>(vx);
                vertex[v].y = static_cast<float>(vy);
                vertex[v].z = static_cast<float>(vz);

                if (idx.normal_index >= 0)
                {
                    auto nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    auto ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    auto nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

                    normal[v].x = static_cast<float>(nx);
                    normal[v].y = static_cast<float>(ny);
                    normal[v].z = static_cast<float>(nz);
                }
                else
                {
                    with_normal = false;
                }

                if (idx.texcoord_index >= 0)
                {
                    auto tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    auto ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

                    uv[v].x = static_cast<float>(tx);
                    uv[v].y = static_cast<float>(ty);
                }
                else
                {
                    with_texcoord = false;
                }
            }
            index_offset += fv;

            if (!with_normal)
            {
                glm::vec3 v0 = vertex[1] - vertex[0];
                glm::vec3 v1 = vertex[2] - vertex[1];
                normal[0]  = glm::normalize( glm::cross(v0, v1));
                normal[1]  = normal[0];
                normal[2]  = normal[0];
            }

            if (!with_texcoord)
            {
                uv[0] = glm::vec2(0.5f, 0.5f);
                uv[1] = glm::vec2(0.5f, 0.5f);
                uv[2] = glm::vec2(0.5f, 0.5f);
            }

            glm::vec3 tangent {1, 0, 0};
            {
                glm::vec3 edge1    = vertex[1] - vertex[0];
                glm::vec3 edge2    = vertex[2] - vertex[1];
                glm::vec2 deltaUV1 = uv[1] - uv[0];
                glm::vec2 deltaUV2 = uv[2] - uv[1];

                auto divide = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
                if (divide >= 0.0f && divide < 0.000001f)
                    divide = 0.000001f;
                else if (divide < 0.0f && divide > -0.000001f)
                    divide = -0.000001f;

                float df  = 1.0f / divide;
                tangent.x = df * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                tangent.y = df * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                tangent.z = df * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                tangent   = glm::normalize(tangent);
            }

            for (size_t i = 0; i < 3; i++)
            {
                Vertex mesh_vert = {};

                mesh_vert.m_position.x = vertex[i].x;
                mesh_vert.m_position.y = vertex[i].y;
                mesh_vert.m_position.z = vertex[i].z;

                mesh_vert.m_normal.x = normal[i].x;
                mesh_vert.m_normal.y = normal[i].y;
                mesh_vert.m_normal.z = normal[i].z;

                mesh_vert.m_uv.x = uv[i].x;
                mesh_vert.m_uv.y = uv[i].y;

                mesh_vert.m_tangent.x = tangent.x;
                mesh_vert.m_tangent.y = tangent.y;
                mesh_vert.m_tangent.z = tangent.z;

                m_vertexData.push_back(mesh_vert);
            }
        }
    }
    
    for (size_t i = 0; i < m_vertexData.size(); i++)
    {
        m_indexData.push_back(static_cast<uint32_t>(i));
    }
}

void ObjLoader::loadCustomSphere()
{
    const int height = 15;
    const int width = 30;
    
    int index = 0;
    for(int j = 0; j < height; ++j)
    {
        int j1 = j;
        int j2 = j+1;
        float v1 = 1.0f*j1/height;
        float v2 = 1.0f*j2/height;
        float sita1 = 3.1415926f * j1/height;
        float sita2 = 3.1415926f * j2/height;
        
        for(int i = 0; i < width; ++i)
        {
            int i1 = i;
            int i2 = i+1;
            
            float u1 = 1.0f*i1/width;
            float u2 = 1.0f*i2/width;
            
            if(i2 == width) i2 = 0;
            
            float alpha1 = 2*3.1415926f * i1/width;
            float alpha2 = 2*3.1415926f * i2/width;
            
            glm::vec3 vec1 = glm::vec3(cos(sita1), sin(sita1)*cos(alpha1), sin(sita1)*sin(alpha1));
            glm::vec3 vec2 = glm::vec3(cos(sita1), sin(sita1)*cos(alpha2), sin(sita1)*sin(alpha2));
            glm::vec3 vec3 = glm::vec3(cos(sita2), sin(sita2)*cos(alpha1), sin(sita2)*sin(alpha1));
            glm::vec3 vec4 = glm::vec3(cos(sita2), sin(sita2)*cos(alpha2), sin(sita2)*sin(alpha2));
            
            Vertex vertex1 = {};
            vertex1.m_position = 0.5f*glm::normalize(vec1);
            vertex1.m_normal = glm::normalize(vec1);
            vertex1.m_uv = glm::vec2(u1,v1);
            
            Vertex vertex2 = {};
            vertex2.m_position = 0.5f*glm::normalize(vec2);
            vertex2.m_normal = glm::normalize(vec2);
            vertex2.m_uv = glm::vec2(u2,v1);
            
            Vertex vertex3 = {};
            vertex3.m_position = 0.5f*glm::normalize(vec3);
            vertex3.m_normal = glm::normalize(vec3);
            vertex3.m_uv = glm::vec2(u1,v2);
            
            Vertex vertex4 = {};
            vertex4.m_position = 0.5f*glm::normalize(vec4);
            vertex4.m_normal = glm::normalize(vec4);
            vertex4.m_uv = glm::vec2(u2,v2);
            
            m_vertexData.push_back(vertex1);
            m_vertexData.push_back(vertex2);
            m_vertexData.push_back(vertex3);
            m_vertexData.push_back(vertex4);
            
            m_indexData.push_back(index + 0);
            m_indexData.push_back(index + 1);
            m_indexData.push_back(index + 2);
            m_indexData.push_back(index + 2);
            m_indexData.push_back(index + 1);
            m_indexData.push_back(index + 3);
            
            index += 4;
        }
    }
}

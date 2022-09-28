
#pragma once

#include "tools.h"

enum VertexComponent { Position, Normal, UV, Color, Tangent, JointIndex, JointWeight };

class Vertex
{
public:
    Vertex();
    ~Vertex();

public:
    glm::vec3 m_position;
    glm::vec3 m_normal = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec2 m_uv = glm::vec2(0.0f);
    glm::vec4 m_color = glm::vec4(1.0f);
    glm::vec4 m_tangent;
    glm::vec4 m_jointIndex;
    glm::vec4 m_jointWeight;

public:
    bool operator==(const Vertex& other) const;
    static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState();
    
public:
    static void setVertexInputBindingDescription(uint32_t binding);
    static void setVertexInputAttributeDescription(uint32_t binding, const std::vector<VertexComponent> components);
    static VkVertexInputAttributeDescription inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);
    static VkVertexInputBindingDescription m_vertexInputBindingDescription;
    static std::vector<VkVertexInputAttributeDescription> m_vertexInputAttributeDescriptions;
};

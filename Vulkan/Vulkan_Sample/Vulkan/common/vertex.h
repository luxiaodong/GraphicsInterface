
#pragma once

#include "tools.h"

enum VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

class Vertex
{
public:
    Vertex();
    ~Vertex();

public:
    glm::vec3 m_position;
    glm::vec3 m_normal;
    glm::vec2 m_uv;
    glm::vec4 m_color;

public:
    static void setVertexInputBindingDescription(uint32_t binding);
    static void setVertexInputAttributeDescription(uint32_t binding, const std::vector<VertexComponent> components);
    static VkVertexInputAttributeDescription inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);
    static VkVertexInputBindingDescription m_vertexInputBindingDescription;
    static std::vector<VkVertexInputAttributeDescription> m_vertexInputAttributeDescriptions;
};

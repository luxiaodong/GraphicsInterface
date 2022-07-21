
#include "vertex.h"

VkVertexInputBindingDescription Vertex::m_vertexInputBindingDescription;
std::vector<VkVertexInputAttributeDescription> Vertex::m_vertexInputAttributeDescriptions;

Vertex::Vertex()
{
}

Vertex::~Vertex()
{}

void Vertex::setVertexInputBindingDescription(uint32_t binding)
{
    Vertex::m_vertexInputBindingDescription = Tools::getVertexInputBindingDescription(binding, sizeof(Vertex));
}

void Vertex::setVertexInputAttributeDescription(uint32_t binding, const std::vector<VertexComponent> components)
{
    m_vertexInputAttributeDescriptions.clear();
    uint32_t location = 0;
    for (VertexComponent component : components)
    {
        m_vertexInputAttributeDescriptions.push_back(Vertex::inputAttributeDescription(binding, location, component));
        location++;
    }
}

VkVertexInputAttributeDescription Vertex::inputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component)
{
    switch (component) {
        case VertexComponent::Position:
            return Tools::getVertexInputAttributeDescription(binding, location, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, m_position) );
        case VertexComponent::Normal:
            return Tools::getVertexInputAttributeDescription(binding, location, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, m_normal) );
        case VertexComponent::UV:
            return Tools::getVertexInputAttributeDescription(binding, location, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, m_uv) );
        case VertexComponent::Color:
            return Tools::getVertexInputAttributeDescription(binding, location, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, m_color) );
        case VertexComponent::Tangent:
            return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, m_tangent)} );
//        case VertexComponent::Joint0:
//            return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, joint0) });
//        case VertexComponent::Weight0:
//            return VkVertexInputAttributeDescription({ location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weight0) });
        default:
            break;
    }
    
    return VkVertexInputAttributeDescription({});
}



#include "skin.h"
#include "gltfNode.h"

Skin::Skin()
{
}

Skin::~Skin()
{}

void Skin::clear()
{
    vkFreeMemory(Tools::m_device, m_jointMatrixMemory, nullptr);
    vkDestroyBuffer(Tools::m_device, m_jointMatrixBuffer, nullptr);
}

void Skin::createJointMatrixBuffer()
{
    m_totalSize = static_cast<uint32_t>(m_joints.size() * sizeof(glm::mat4));
    Tools::createBufferAndMemoryThenBind(m_totalSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         m_jointMatrixBuffer, m_jointMatrixMemory);    
    
    for(int i = 0; i < m_joints.size(); ++i)
    {
        m_jointMatrices.push_back(glm::mat4(1.0f));
    }
}

void Skin::update()
{
    for(int i = 0; i < m_joints.size(); ++i)
    {
        GltfNode* pNode = m_joints.at(i);
        m_jointMatrices[i] = pNode->m_worldMatrix * m_inverseBindMatrices.at(i);
    }
    
    Tools::mapMemory(m_jointMatrixMemory, m_totalSize, m_jointMatrices.data());
}

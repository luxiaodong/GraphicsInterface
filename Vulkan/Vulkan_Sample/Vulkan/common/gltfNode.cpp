
#include "gltfNode.h"

GltfNode::GltfNode()
{
}

GltfNode::~GltfNode()
{}

glm::mat4 GltfNode::localMatrix()
{
    return glm::translate(glm::mat4(1.0f), m_translation) * glm::mat4(m_rotation) * glm::scale(glm::mat4(1.0f), m_scale) * m_originMat;
}

glm::mat4 GltfNode::worldMatrix()
{
    glm::mat4 m = localMatrix();
    GltfNode *p = m_parent;
    while (p)
    {
        m = p->localMatrix() * m;
        p = p->m_parent;
    }
    return m;
}


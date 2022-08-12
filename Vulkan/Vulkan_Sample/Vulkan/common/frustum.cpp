
#include "frustum.h"

void Frustum::update(glm::mat4 matrix)
{
    m_planes[LEFT].x = matrix[0].w + matrix[0].x;
    m_planes[LEFT].y = matrix[1].w + matrix[1].x;
    m_planes[LEFT].z = matrix[2].w + matrix[2].x;
    m_planes[LEFT].w = matrix[3].w + matrix[3].x;

    m_planes[RIGHT].x = matrix[0].w - matrix[0].x;
    m_planes[RIGHT].y = matrix[1].w - matrix[1].x;
    m_planes[RIGHT].z = matrix[2].w - matrix[2].x;
    m_planes[RIGHT].w = matrix[3].w - matrix[3].x;

    m_planes[TOP].x = matrix[0].w - matrix[0].y;
    m_planes[TOP].y = matrix[1].w - matrix[1].y;
    m_planes[TOP].z = matrix[2].w - matrix[2].y;
    m_planes[TOP].w = matrix[3].w - matrix[3].y;

    m_planes[BOTTOM].x = matrix[0].w + matrix[0].y;
    m_planes[BOTTOM].y = matrix[1].w + matrix[1].y;
    m_planes[BOTTOM].z = matrix[2].w + matrix[2].y;
    m_planes[BOTTOM].w = matrix[3].w + matrix[3].y;

    m_planes[BACK].x = matrix[0].w + matrix[0].z;
    m_planes[BACK].y = matrix[1].w + matrix[1].z;
    m_planes[BACK].z = matrix[2].w + matrix[2].z;
    m_planes[BACK].w = matrix[3].w + matrix[3].z;

    m_planes[FRONT].x = matrix[0].w - matrix[0].z;
    m_planes[FRONT].y = matrix[1].w - matrix[1].z;
    m_planes[FRONT].z = matrix[2].w - matrix[2].z;
    m_planes[FRONT].w = matrix[3].w - matrix[3].z;

    // 单位化
    for (auto i = 0; i < m_planes.size(); i++)
    {
        float length = sqrtf(m_planes[i].x * m_planes[i].x + m_planes[i].y * m_planes[i].y + m_planes[i].z * m_planes[i].z);
        m_planes[i] /= length;
    }
}

//这段要理解下这个平面在哪个空间里面,感觉在世界空间
bool Frustum::checkSphere(glm::vec3 pos, float radius)
{
    for (auto i = 0; i < m_planes.size(); i++)
    {
        if ((m_planes[i].x * pos.x) + (m_planes[i].y * pos.y) + (m_planes[i].z * pos.z) + m_planes[i].w + radius <= 0)
        {
            return false;
        }
    }
    return true;
}


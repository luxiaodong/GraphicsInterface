
#pragma once

#include "tools.h"

class Frustum
{
public:
    void update(glm::mat4 matrix);
    bool checkSphere(glm::vec3 pos, float radius);
    
public:
    enum Plane_Side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
    std::array<glm::vec4, 6> m_planes;
};

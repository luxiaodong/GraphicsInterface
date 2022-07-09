
#pragma once

#include "tools.h"
#include "primitive.h"

class Mesh
{
public:
    Mesh();
    ~Mesh();
    
public:
    std::string m_name;
    glm::mat4 m_matrix;
    std::vector<Primitive*> m_primitives;
};

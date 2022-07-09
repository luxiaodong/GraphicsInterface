
#pragma once

#include "tools.h"

class Primitive
{
public:
    Primitive();
    ~Primitive();
    
public:
    uint32_t m_vertexOffset;
    uint32_t m_vertexCount;
    uint32_t m_indexOffset;
    uint32_t m_indexCount;
};


#include "material.h"

Material::Material()
{
}

Material::~Material()
{}

void Material::clear()
{
    if(m_isNeedVkBuffer)
    {
        vkDestroyPipeline(Tools::m_device, m_graphicsPipeline, nullptr);
    }
}



#include "gltfLoader.h"

GltfLoader::GltfLoader()
{
    m_pModel = new vkglTF::Model();
}

GltfLoader::~GltfLoader()
{}

void GltfLoader::clear()
{
    if(m_pModel)
    {
        delete m_pModel;
    }
}

void GltfLoader::loadFromFile(std::string fileName, VkQueue transferQueue)
{
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::DontLoadImages;
    m_pModel->loadFromFile(fileName, transferQueue, glTFLoadingFlags);
}

void GltfLoader::bindBuffers(VkCommandBuffer commandBuffer)
{
    m_pModel->bindBuffers(commandBuffer);
}

void GltfLoader::draw(VkCommandBuffer commandBuffer)
{
    m_pModel->draw(commandBuffer);
}


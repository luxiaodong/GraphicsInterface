
#include "descriptorsets.h"

Descriptorsets::Descriptorsets(std::string title) : Application(title)
{
}

Descriptorsets::~Descriptorsets()
{}

void Descriptorsets::init()
{
    Application::init();

    prepareVertex();
    prepareUniform();
    prepareDescriptorSetLayoutAndPipelineLayout();
    prepareDescriptorSetAndWrite();
    createGraphicsPipeline();
}

void Descriptorsets::initCamera()
{
    m_camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));
    m_camera.setRotation(glm::vec3(0.0f));
    m_camera.setPerspective(60.0f, (float)m_width / (float)m_height, 0.1f, 512.0f);
}

void Descriptorsets::setEnabledFeatures()
{
    if( m_deviceFeatures.samplerAnisotropy )
    {
        m_deviceEnabledFeatures.samplerAnisotropy = VK_TRUE;
    }
}

void Descriptorsets::clear()
{
//    vkFreeMemory(m_device, m_vertexMemory, nullptr);
//    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
//    vkFreeMemory(m_device, m_indexMemory, nullptr);
//    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
//    vkFreeMemory(m_device, m_uniformMemory, nullptr);
//    vkDestroyBuffer(m_device, m_uniformBuffer, nullptr);
//
//    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
//    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
//
//    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
//    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

    m_gltfLoader.clear();
    Application::clear();
}

void Descriptorsets::prepareVertex()
{
    m_gltfLoader.loadFromFile(Tools::getModelPath() + "cube.gltf", m_graphicsQueue);
}

void Descriptorsets::prepareUniform()
{}

void Descriptorsets::prepareDescriptorSetLayoutAndPipelineLayout()
{}

void Descriptorsets::prepareDescriptorSetAndWrite()
{}

void Descriptorsets::createGraphicsPipeline()
{}

void Descriptorsets::prepareRenderData()
{
}

void Descriptorsets::recordRenderCommand(const VkCommandBuffer commandBuffer)
{
}


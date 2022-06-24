
#pragma once

#include "common/application.h"

class Hello_triangle : public Application
{
public:
    Hello_triangle(std::string title);
    virtual ~Hello_triangle();
    
    virtual void init();
    virtual void clear();
    virtual void render();
    
protected:
    void createPipelineLayout();
//    void createRenderPass();
    void createGraphicsPipeline();
    void createCommandBuffers();
    void recordCommandBuffers();
    void createSemaphores();
    
protected:
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
//    VkRenderPass m_renderPass;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
};

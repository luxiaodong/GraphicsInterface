
#pragma once

#include "common/application.h"

class Hello_triangle : public Application
{
public:
    Hello_triangle(std::string title);
    virtual ~Hello_triangle();
    
    virtual void init();
    virtual void clear();
    virtual void recordRenderCommand(const VkCommandBuffer commandBuffer);
    
protected:
    void createPipelineLayout();
    void createGraphicsPipeline();
    void recordCommandBuffers();

protected:
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
};

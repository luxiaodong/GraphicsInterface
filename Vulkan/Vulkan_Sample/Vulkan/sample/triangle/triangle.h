
#pragma once

#include "common/application.h"

class Triangle : public Application
{
public:
    Triangle(std::string title);
    virtual ~Triangle();
    
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

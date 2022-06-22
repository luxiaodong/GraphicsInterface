
#pragma once

#include "imgui.h"
#include "tools.h"

class Ui
{
public:
    Ui();
    void prepareResources();

public:
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    
private:
    VkImage m_fontImage = VK_NULL_HANDLE;
    VkDeviceMemory m_fontMemory = VK_NULL_HANDLE;
    VkImageView m_fontImageView = VK_NULL_HANDLE;
    VkSampler m_fontSampler = VK_NULL_HANDLE;
};

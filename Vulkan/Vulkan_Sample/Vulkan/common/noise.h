
#pragma once

#include "tools.h"

class PerlinNoise
{
public:
    PerlinNoise();
    float fade(float t);
    float lerp(float t, float a, float b);
    float grad(int hash, float x, float y, float z);
    float noise(float x, float y, float z);
    
private:
    uint32_t m_permutations[512];
};


class FractalNoise
{
public:
    FractalNoise(const PerlinNoise& perlinNoise);
    float noise(float x, float y, float z);
    
private:
    PerlinNoise m_perlinNoise;
    uint32_t m_octaves;
    float m_persistence;
//    float m_frequency;
//    float m_amplitude;
};


class Noise
{
public:
    Noise();
    ~Noise();

public:
    static Noise* createNoise3D(uint32_t width, uint32_t height, uint32_t depth, VkQueue transferQueue);

    void clear();
    VkDescriptorImageInfo getDescriptorImageInfo();

public:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_depth;
    uint32_t m_mipLevels;
    uint32_t m_layerCount;

    VkImageLayout   m_imageLayout;
    VkFormat        m_fromat;
    VkImage         m_image;
    VkDeviceMemory  m_imageMemory;
    VkImageView     m_imageView;
    VkSampler       m_sampler;    
};

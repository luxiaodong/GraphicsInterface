
#pragma once

#include "tools.h"
#include <limits.h>

class GltfNode;

enum AnimationChannelType {Translation, Rotation, Scale};
enum AnimationSamplerType {Linear, Step, CubicSpline};

class AnimationChannel
{
public:
    AnimationChannelType m_channelType;
    GltfNode* m_node;
    uint32_t m_samplerIndex;
};

class AnimationSampler
{
public:
    AnimationSamplerType m_samplerType;
    std::vector<float> m_keyFrames; //时间轴上的关键帧
    std::vector<glm::vec4> m_values; //关键帧上的数据,position数据,rotate数据,scale数据
};

class Animation
{
public:
    Animation();
    ~Animation();
    
public:
    std::string m_name;
    std::vector<AnimationChannel> m_channels;
    std::vector<AnimationSampler> m_samplers;
    float m_start = std::numeric_limits<float>::max();
    float m_end = std::numeric_limits<float>::min();
    float m_currentTime = 0.0f;
};

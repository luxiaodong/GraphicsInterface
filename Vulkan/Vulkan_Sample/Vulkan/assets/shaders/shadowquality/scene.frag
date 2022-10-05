#version 450

layout (binding = 1) uniform sampler2D shadowMap;

layout (binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    mat4 model;
    mat4 lightSpace;
    vec4 lightPos;
    float zNear;
    float zFar;
} ubo;

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;

layout (constant_id = 0) const int enablePCF = 0;

layout (location = 0) out vec4 outFragColor;

float textureProj(vec4 shadowCoord, vec2 offset)
{
    float closest_depth = texture(shadowMap, shadowCoord.xy + offset).r;
    float current_depth = shadowCoord.z;
    return closest_depth >= current_depth ? 1.0f : 0.0f;
}

const mat4 biasMat = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);


void main()
{
    vec3 Lo = vec3(0.0, 0.0, 0.0);
    vec3 N = normalize(inNormal);
    vec3 L = normalize( vec3(1,-1,1) );

    float NoL = min(dot(N, L), 1.0);

    if(NoL > 0.0f)
    {
        Lo = inColor;
        Lo *= NoL;
        
        vec4 position_clip = biasMat * ubo.lightSpace * vec4(inWorldPos, 1.0);
        vec4 shadowCoord  = position_clip / position_clip.w;
        float shadow = textureProj(shadowCoord, vec2(0.0));
        
        if(shadow > 0.0f)
        {
            Lo *= shadow;
        }
        else
        {
            Lo = vec3(0.0, 0.0, 0.0);
        }
    }
    
    outFragColor = vec4(Lo, 1.0);
}

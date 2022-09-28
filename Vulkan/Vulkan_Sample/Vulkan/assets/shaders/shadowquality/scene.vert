#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;

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

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;

void main() 
{
    outWorldPos = inPos;
    gl_Position = ubo.projection * ubo.view * vec4(outWorldPos, 1.0);
    outNormal = normalize(inNormal);
}

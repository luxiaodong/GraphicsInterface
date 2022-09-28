#version 450

layout(early_fragment_tests) in;

layout(location = 0) out float out_depth;

void main()
{
    out_depth = gl_FragCoord.z;
}

//layout(location = 0) out vec4 color;
//
//void main()
//{
//	color = vec4(1.0, 0.0, 0.0, 1.0);
//}

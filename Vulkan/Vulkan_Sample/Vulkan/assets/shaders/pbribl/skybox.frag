#version 450

layout (binding = 2) uniform samplerCube samplerEnv;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform UBOParams {
	vec4 lights[4];
	float exposure;
	float gamma;
} uboParams;

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec3 getSamplePos(vec2 inUV)
{
    vec3 samplePos = vec3(0.0f);
    
    // Crude statement to visualize different cube map faces based on UV coordinates
    int x = int(floor(inUV.x / 0.25f));
    int y = int(floor(inUV.y / (1.0 / 3.0)));
    if (y == 1) {
        vec2 uv = vec2(inUV.x * 4.0f, (inUV.y - 1.0/3.0) * 3.0);
        uv = 2.0 * vec2(uv.x - float(x) * 1.0, uv.y) - 1.0;
        switch (x) {
            case 0:    // NEGATIVE_X
                samplePos = vec3(-1.0f, uv.y, uv.x);
                break;
            case 1: // POSITIVE_Z
                samplePos = vec3(uv.x, uv.y, 1.0f);
                break;
            case 2: // POSITIVE_X
                samplePos = vec3(1.0, uv.y, -uv.x);
                break;
            case 3: // NEGATIVE_Z
                samplePos = vec3(-uv.x, uv.y, -1.0f);
                break;
        }
    } else {
        if (x == 1) {
            vec2 uv = vec2((inUV.x - 0.25) * 4.0, (inUV.y - float(y) / 3.0) * 3.0);
            uv = 2.0 * uv - 1.0;
            switch (y) {
                case 0: // NEGATIVE_Y
                    samplePos = vec3(uv.x, -1.0f, uv.y);
                    break;
                case 2: // POSITIVE_Y
                    samplePos = vec3(uv.x, 1.0f, -uv.y);
                    break;
            }
        }
    }
    
    return samplePos;
}

void main() 
{
	vec3 color = texture(samplerEnv, inUVW).rgb;
//    vec3 color = texture(samplerEnv, getSamplePos(inUVW.xy)).rgb;

	// Tone mapping
	 color = Uncharted2Tonemap(color * uboParams.exposure);
	 color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
     color = pow(color, vec3(1.0f / uboParams.gamma));

	outColor = vec4(color, 1.0);
}

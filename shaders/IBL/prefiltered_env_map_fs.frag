#version 460 core

// ===== Fragment Inputs =====
in VS_OUT {
    vec3 uv;
} fs_in;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources (keep bindings stable across shaders) =====

layout(binding = 0) uniform samplerCube u_EnvMap;  

// ===== Uniforms (small scalar params; use UBO if grows) =====
layout(location = 0) uniform int u_SampleCount;
layout(location = 1) uniform float u_Roughness;

// ===== Helpers =====

const float PI = 3.1415926;


// Hammersley
float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N + 1), RadicalInverse_VdC(i));
} 

// coord base N -> (b1, b2, N)
void GetONB(in vec3 n, out vec3 b1, out vec3 b2)
{
    float s = mix(-1.0, 1.0, step(0.0, n.z)); // n.z>=0 -> 1, 否则 -1

    float a = -1.0 / (s + n.z);
    float b = n.x * n.y * a;

    b1 = vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
    b2 = vec3(b, s + n.y * n.y * a,  -n.y);
}

vec3 ImportanceSampleGGX(vec2 Xi, float a2, vec3 N)
{
    float phi = 2 * PI * Xi.x;
    float cos_theta = sqrt((1 - Xi.y) / (1 + (a2 - 1) * Xi.y));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);
    
    vec3 H = vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

    vec3 b1, b2;
    GetONB(N, b1, b2);
    // TS -> WS
    return b1 * H.x + b2 * H.y + N * H.z;
}

void main()
{
    vec3 N = normalize(fs_in.uv);

    // 完美镜面反射，无需多次采样
    if (u_Roughness < 1e-4) 
    {
        o_Color = vec4(texture(u_EnvMap, N).rgb, 1.0);
        return;
    }

    // mipmap filtered samples
    ivec2 env_map_size = textureSize(u_EnvMap, 0);
    int W = env_map_size.x;
    float Omega_p = 4.0 * PI / (6.0 * float(W) * float(W));
    int mipCount = textureQueryLevels(u_EnvMap);
    float maxMip = float(mipCount - 1);



    float total_weight = 0.0;
    vec3 sum = vec3(0.0);

    float a  = u_Roughness * u_Roughness;   // alpha
    float a2 = a * a;                        // alpha * alpha
    for (int i = 0; i < u_SampleCount; i ++ )
    {
        vec2 Xi = Hammersley(i, u_SampleCount);
        vec3 H = ImportanceSampleGGX(Xi, a2, N);
        vec3 L = reflect(-N, H);
        float NdotL = max(0.0, dot(N, L));
        // 跳过背面采样
        if (NdotL > 0.0)
        {
            float NdotH = max(0.0, dot(N, H));
            float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
            float pdf_li = a2 / (4.0 * PI * denom * denom);
            float Omega_s = 1.0 / (float(u_SampleCount) * pdf_li + 1e-6);
            float mip_level = clamp(0.5 * log2(Omega_s / Omega_p), 0.0, maxMip);
            sum += textureLod(u_EnvMap, L, mip_level).rgb * NdotL;
            total_weight += NdotL;
        }
    }
        
    float invW = 1.0 / max(total_weight, 1e-6);
    o_Color = vec4(sum * invW, 1.0);
}
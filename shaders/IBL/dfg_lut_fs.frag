#version 460 core

// IBL DFG LUT（预计算 BRDF 项，用于 split-sum 近似）
// 渲染到全屏 quad 上，x 轴 = NdotV，y 轴 = perceptual roughness
//
// 固定 N = (0,0,1)，各向同性 BRDF 下结果与 N 的具体方向无关
// 蒙特卡洛估计量（single-scatter）:
//   fc     = (1 - VdotH)^5               — Fresnel Schlick 权重
//   weight = Vis * VdotH * NdotL / NdotH — 其中 Vis = G/(4*NdotV*NdotL)
//   DFG1   = 4/N * Σ((1 - fc) * weight)
//   DFG2   = 4/N * Σ(fc * weight)
// o_Color.xy = (DFG1, DFG2)

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Uniforms (small scalar params; use UBO if grows) =====
layout(location = 0) uniform int u_SampleCount;
layout(location = 1) uniform int u_LUTSize;

// ===== Helpers =====
const float PI = 3.1415926;

// Helper 函数，被 Hammersley() 调用
float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley 序列生成，其中 i 是样本索引，N 是总样本数
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N + 1), RadicalInverse_VdC(i));
} 

// 各向同性BRDF，N 可以随意设置
// 所以 N 固定为 (0,0,1)，切线空间即世界空间，无需 ONB 变换
vec3 ImportanceSampleGGX(vec2 Xi, float a2)
{
    float phi = 2.0 * PI * Xi.x;
    float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    return vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
}

// Smith GGX Visibility term：Vis = G / (4 * NdotV * NdotL)，已包含 BRDF 分母约分
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2) 
{
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL + 1e-7);
}

// Fresnel Schlick 权重：(1 - VdotH)^5，用乘法展开代替 pow()
float FresnelWeight(float VdotH)
{
    float x = 1.0 - VdotH;
    float x2 = x * x;
    float x4 = x2 * x2;
    return x4 * x;
}


void main()
{
    vec2 uv = gl_FragCoord.xy / float(u_LUTSize);

    // x 轴: NdotV,  y 轴: perceptual roughness
    float NdotV = max(uv.x, 1e-4);  // 避免 NdotV=0 时 V 退化
    float roughness = uv.y;
    float a  = roughness * roughness;
    float a2 = a * a;

    // 固定 N = (0,0,1)，从 NdotV 反推 V（取 V.y = 0 的解）
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    float dfg1 = 0.0, dfg2 = 0.0;

    for (int i = 0; i < u_SampleCount; i++)
    {
        vec2 Xi = Hammersley(uint(i), uint(u_SampleCount));
        vec3 H = ImportanceSampleGGX(Xi, a2);
        vec3 L = reflect(-V, H);

        float NdotL = L.z;  // N = (0,0,1)
        if (NdotL > 0.0)
        {
            float NdotH = max(H.z, 0.0); // N = (0,0,1)
            float VdotH = max(dot(V, H), 0.0);

            float Vis   = V_SmithGGXCorrelated(NdotV, NdotL, a2);
            float fc    = FresnelWeight(VdotH);
            float weight = Vis * VdotH * NdotL / (NdotH + 1e-7);

            dfg1 += (1.0 - fc) * weight;
            dfg2 += fc * weight;
        }
    }

    dfg1 = 4.0 * dfg1 / float(u_SampleCount);
    dfg2 = 4.0 * dfg2 / float(u_SampleCount);

    o_Color = vec4(dfg1, dfg2, 0.0, 1.0);
}

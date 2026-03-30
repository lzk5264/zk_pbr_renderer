#version 460 core

// IBL Prefiltered Environment Map（配合 capture_cubemap_vs.vert 使用）
//
// Split-sum 近似中的 LD 项：对环境光做 GGX 加权卷积。
// 每个 mip level 对应一个粗糙度（roughness = mip / maxMip），由 C++ 端逐级调用。
//
// 近似假设：V = R = N（预计算时不知道实际视线方向），因此 L·H = N·H。
//
// 蒙特卡洛估计量：LD = Σ(Li * NdotL) / Σ(NdotL)
// 其中 Li 来自 mipmap filtered samples，mip level 由采样 PDF 对应的立体角决定。

// ===== Fragment Inputs =====
in vec3 v_LocalPos;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources (keep bindings stable across shaders) =====
layout(binding = 0) uniform samplerCube u_EnvMap;  

// ===== Uniforms (small scalar params; use UBO if grows) =====
layout(location = 0) uniform int u_SampleCount;
layout(location = 1) uniform float u_Roughness;

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

// pixar 正交基构造方法
// 输入 n 必须是单位向量
// 输出 b1, b2 与 n 构成正交基：{b1, b2, n}
void GetONB(in vec3 n, out vec3 b1, out vec3 b2)
{
    float s = mix(-1.0, 1.0, step(0.0, n.z)); // n.z>=0 -> 1, 否则 -1

    float a = -1.0 / (s + n.z);
    float b = n.x * n.y * a;

    b1 = vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
    b2 = vec3(b, s + n.y * n.y * a,  -n.y);
}



// GGX 重要性采样，返回 world space 半程向量 H
// 按 D_GGX(H) * NdotH 分布采样 H，再由调用者通过 reflect(-N, H) 得到 L
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
    vec3 N = normalize(v_LocalPos);

    // 完美镜面反射，无需多次采样
    if (u_Roughness < 1e-4) 
    {
        o_Color = vec4(texture(u_EnvMap, N).rgb, 1.0);
        return;
    }

    // mipmap filtered samples：用 textureLod 在合适的 mip level 采样，
    // 等效于对该 solid angle 范围内的 texel 做预滤波，减少高粗糙度下的噪点.
    ivec2 env_map_size = textureSize(u_EnvMap, 0);
    int W = env_map_size.x;
    float Omega_p = 4.0 * PI / (6.0 * float(W) * float(W)); // 单个 texel 对应的立体角

    int mipCount = textureQueryLevels(u_EnvMap);
    float maxMip = float(mipCount - 1);

    float a  = u_Roughness * u_Roughness;   // alpha
    float a2 = a * a;                        // alpha * alpha

    float total_weight = 0.0;
    vec3 sum = vec3(0.0);

    for (int i = 0; i < u_SampleCount; i ++ )
    {
        vec2 Xi = Hammersley(i, u_SampleCount);
        vec3 H = ImportanceSampleGGX(Xi, a2, N);
        vec3 L = reflect(-N, H);
        float NdotL = max(0.0, dot(N, L));
        // 跳过背面采样
        if (NdotL > 0.0)
        {
            // 计算采样对应的 mip level
            //   pdf_H  = D_GGX * NdotH = α² * NdotH / (π * denom²)
            //   pdf_L  = pdf_H / (4 * LdotH)，V=N 近似下 LdotH = NdotH，故约掉
            //   pdf_L  = α² / (4π * denom²)
            //   Ωs     = 1 / (N * pdf_L)  — 单个采样对应的立体角
            //   mip    = 0.5 * log2(Ωs / Ωp) — Ωs 越大（PDF 越小）取越高 mip
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
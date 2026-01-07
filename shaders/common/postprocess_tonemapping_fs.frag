#version 460 core

// ============================================================================
// 后处理片段着色器 - Tone Mapping + Gamma 校正
// - 从 HDR framebuffer 读取颜色
// - 应用 tone mapping 将 HDR 转换为 LDR
// - 应用 gamma 校正
// ============================================================================

// ===== Fragment Inputs =====
in VS_OUT {
    vec2 texCoord;
} fs_in;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources =====
layout(binding = 0) uniform sampler2D u_HDRBuffer;

// ===== Uniforms =====
layout(location = 0) uniform float u_Exposure = 1.0;
layout(location = 1) uniform float u_Gamma = 2.2;

// ===== Tone Mapping Algorithms =====

// ACES Filmic Tone Mapping（电影级，推荐）
// 来源：Stephen Hill (2015), "Aces Filmic Tone Mapping Curve"
vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard Tone Mapping（简单）
vec3 Reinhard(vec3 hdr)
{
    return hdr / (hdr + vec3(1.0));
}

// Uncharted 2 Tone Mapping（游戏常用）
vec3 Uncharted2Tonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 Uncharted2(vec3 color)
{
    float W = 11.2;
    color = Uncharted2Tonemap(color * 2.0);
    vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
    return color * whiteScale;
}

void main()
{
    // 采样 HDR 颜色
    vec3 hdrColor = texture(u_HDRBuffer, fs_in.texCoord).rgb;
    
    // 应用曝光调整
    hdrColor *= u_Exposure;
    
    // Tone Mapping: HDR -> LDR
    vec3 ldrColor = ACESFilm(hdrColor);
    // 可以切换到其他 tone mapping 算法：
    // vec3 ldrColor = Reinhard(hdrColor);
    // vec3 ldrColor = Uncharted2(hdrColor);
    
    // Gamma 校正
    ldrColor = pow(ldrColor, vec3(1.0 / u_Gamma));
    
    o_Color = vec4(ldrColor, 1.0);
}

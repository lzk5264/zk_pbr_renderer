#version 460 core

// ============================================================================
// Bloom Downsample - 片段着色器
// 使用 13-tap 滤波器进行高质量降采样（来自 Call of Duty: Advanced Warfare）
// 第一次降采样时同时做亮度提取（brightness threshold）
// ============================================================================

in VS_OUT {
    vec2 texCoord;
} fs_in;

layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_SourceTex;

layout(location = 0) uniform vec2  u_SrcTexelSize;  // 1.0 / sourceResolution
layout(location = 1) uniform bool  u_FirstPass;     // 第一次降采样时做亮度提取
layout(location = 2) uniform float u_Threshold;     // 亮度阈值 (仅 firstPass 有效)

// Karis average: 防止第一次降采样时高亮像素导致闪烁 (firefly artifact)
// 参考: Brian Karis, "Real Shading in Unreal Engine 4", SIGGRAPH 2013
float karisWeight(vec3 c)
{
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return 1.0 / (1.0 + luma);
}

// 亮度提取：soft knee threshold
vec3 thresholdFilter(vec3 color)
{
    float brightness = max(color.r, max(color.g, color.b));
    float soft = brightness - u_Threshold + 0.5; // knee = 0.5
    soft = clamp(soft, 0.0, 1.0);
    soft = soft * soft * (1.0 / 1.0); // quadratic falloff
    float contribution = max(soft, brightness - u_Threshold) / max(brightness, 1e-4);
    return color * max(contribution, 0.0);
}

void main()
{
    vec2 uv = fs_in.texCoord;
    vec2 d = u_SrcTexelSize;

    // 13-tap downsample filter (4 bilinear fetches at box corners + center samples)
    //
    //  a - b - c
    //  | / | \ |
    //  d - e - f
    //  | \ | / |
    //  g - h - i
    //
    // 权重分布: e = 4/16, (b,d,f,h) = 2/16 each, (a,c,g,i) = 1/16 each
    // 但我们用 bilinear 采样优化，减少采样次数

    vec3 a = texture(u_SourceTex, uv + vec2(-1.0, -1.0) * d).rgb;
    vec3 b = texture(u_SourceTex, uv + vec2( 0.0, -1.0) * d).rgb;
    vec3 c = texture(u_SourceTex, uv + vec2( 1.0, -1.0) * d).rgb;
    vec3 dd = texture(u_SourceTex, uv + vec2(-1.0,  0.0) * d).rgb;
    vec3 e = texture(u_SourceTex, uv).rgb;
    vec3 f = texture(u_SourceTex, uv + vec2( 1.0,  0.0) * d).rgb;
    vec3 g = texture(u_SourceTex, uv + vec2(-1.0,  1.0) * d).rgb;
    vec3 h = texture(u_SourceTex, uv + vec2( 0.0,  1.0) * d).rgb;
    vec3 i = texture(u_SourceTex, uv + vec2( 1.0,  1.0) * d).rgb;

    // 额外 4 个对角半像素采样
    vec3 j = texture(u_SourceTex, uv + vec2(-0.5, -0.5) * d).rgb;
    vec3 k = texture(u_SourceTex, uv + vec2( 0.5, -0.5) * d).rgb;
    vec3 l = texture(u_SourceTex, uv + vec2(-0.5,  0.5) * d).rgb;
    vec3 m = texture(u_SourceTex, uv + vec2( 0.5,  0.5) * d).rgb;

    vec3 color;

    if (u_FirstPass)
    {
        // 使用 Karis average 防止 firefly
        vec3 g0 = (a + b + dd + e) * 0.25;
        vec3 g1 = (b + c + e + f) * 0.25;
        vec3 g2 = (dd + e + g + h) * 0.25;
        vec3 g3 = (e + f + h + i) * 0.25;

        float w0 = karisWeight(g0);
        float w1 = karisWeight(g1);
        float w2 = karisWeight(g2);
        float w3 = karisWeight(g3);

        color = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3) / (w0 + w1 + w2 + w3);
        color = thresholdFilter(color);
    }
    else
    {
        // 标准 13-tap 降采样
        color  = e * 0.125;
        color += (b + dd + f + h) * 0.0625;
        color += (a + c + g + i) * 0.03125;
        color += (j + k + l + m) * 0.125;
    }

    o_Color = vec4(color, 1.0);
}

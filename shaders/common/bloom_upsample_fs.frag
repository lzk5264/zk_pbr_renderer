#version 460 core

// ============================================================================
// Bloom Upsample - 片段着色器
// 使用 9-tap tent filter 进行高质量升采样
// 与上一级 mip 做加法混合，逐级向上累积 bloom
// ============================================================================

in VS_OUT {
    vec2 texCoord;
} fs_in;

layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_SourceTex;  // 当前 mip (较低分辨率)

layout(location = 0) uniform vec2  u_SrcTexelSize;  // 1.0 / sourceResolution
layout(location = 1) uniform float u_BloomIntensity; // bloom 混合强度

void main()
{
    vec2 uv = fs_in.texCoord;
    vec2 d = u_SrcTexelSize;

    // 9-tap tent filter (3x3 bilinear)
    //  1  2  1
    //  2  4  2  / 16
    //  1  2  1
    vec3 color = vec3(0.0);
    color += texture(u_SourceTex, uv + vec2(-1.0, -1.0) * d).rgb * 1.0;
    color += texture(u_SourceTex, uv + vec2( 0.0, -1.0) * d).rgb * 2.0;
    color += texture(u_SourceTex, uv + vec2( 1.0, -1.0) * d).rgb * 1.0;
    color += texture(u_SourceTex, uv + vec2(-1.0,  0.0) * d).rgb * 2.0;
    color += texture(u_SourceTex, uv                          ).rgb * 4.0;
    color += texture(u_SourceTex, uv + vec2( 1.0,  0.0) * d).rgb * 2.0;
    color += texture(u_SourceTex, uv + vec2(-1.0,  1.0) * d).rgb * 1.0;
    color += texture(u_SourceTex, uv + vec2( 0.0,  1.0) * d).rgb * 2.0;
    color += texture(u_SourceTex, uv + vec2( 1.0,  1.0) * d).rgb * 1.0;
    color /= 16.0;

    o_Color = vec4(color * u_BloomIntensity, 1.0);
}

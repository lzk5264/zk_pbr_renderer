#version 460 core

// ============================================================================
// - Naming: fs_in.* (varyings), u_* (uniforms), o_* (outputs)
// - Resources use layout(binding=...) for stable CPU-side binding
// ============================================================================

// ===== Feature Toggles (compile-time) =====
// #define USE_ALBEDO_MAP 1
// #define DEBUG_UV 0

// ===== Fragment Inputs =====
in VS_OUT {
    vec3 posOS;
} fs_in;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources (keep bindings stable across shaders) =====
layout(binding = 0) uniform sampler2D u_EquirectMap;

// 等距矩形贴图 UV 计算
// 将方向向量转换为球面坐标 (经度, 纬度)
vec2 SampleSphericalMap(vec3 v)
{
    // 归一化方向向量
    vec3 dir = normalize(v);
    
    // 计算球面坐标
    // atan(y, x) 返回 [-PI, PI]
    // asin(z) 返回 [-PI/2, PI/2]
    vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
    
    // 归一化到 [0, 1] 范围
    uv *= vec2(0.1591, 0.3183);  // 1/(2*PI), 1/PI
    uv += 0.5;
    
    return uv;
}

void main()
{
    // 采样等距矩形贴图
    vec3 hdrColor = texture(u_EquirectMap, SampleSphericalMap(fs_in.posOS)).rgb;
    
    // 应用曝光调整（可选）
    float exposure = 1.0;
    hdrColor *= exposure;
    
    // 直接输出 HDR 颜色
    // NOTE: Tone mapping 和 gamma 校正现在应该在后处理 pass 中统一处理
    o_Color = vec4(hdrColor, 1.0);
}

#version 460 core

// ============================================================================
// Template Fragment Shader
// - Naming: fs_in.* (varyings), u_* (uniforms), o_* (outputs)
// - Resources use layout(binding=...) for stable CPU-side binding
// ============================================================================

// ===== Feature Toggles (compile-time) =====
// #define USE_ALBEDO_MAP 1
// #define DEBUG_UV 0

// ===== Fragment Inputs =====
in VS_OUT {
    vec2 uv;
    vec3 normalWS;
    vec3 posWS;
} fs_in;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources (keep bindings stable across shaders) =====
layout(binding = 0) uniform sampler2D u_AlbedoTex;  // default: white 1x1
// layout(binding = 1) uniform sampler2D u_NormalTex; // default: (0.5,0.5,1)
// layout(binding = 2) uniform sampler2D u_MRATex;    // metallic/roughness/ao etc.

// ===== Uniforms (small scalar params; use UBO if grows) =====
layout(location = 0) uniform vec4 u_BaseColor = vec4(1.0); // if your compiler dislikes init, set from CPU

// ===== Helpers =====
vec3 safeNormalize(vec3 v)
{
    return (dot(v, v) > 0.0) ? normalize(v) : vec3(0.0, 0.0, 1.0);
}

void main()
{
    // Debug patterns you can quickly switch to:
    // o_Color = vec4(fract(fs_in.uv * 10.0), 0.0, 1.0); return;

    vec4 albedo = texture(u_AlbedoTex, fs_in.uv) * u_BaseColor;

    // Minimal shading placeholder (unlit):
    o_Color = albedo;
}

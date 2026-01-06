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
    vec3 uv;
} fs_in;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources (keep bindings stable across shaders) =====
layout(binding = 0) uniform samplerCube u_Cubemap;

void main()
{

    // Minimal shading placeholder (unlit):
    o_Color = texture(u_Cubemap, fs_in.uv);
}

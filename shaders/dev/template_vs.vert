#version 460 core

// ============================================================================
// Template Vertex Shader
// - Naming: a_* (attributes), u_* (uniforms), vs_out.* (varyings)
// - Spaces: OS (object), WS (world), VS (view), CS (clip)
// ============================================================================

// ===== Feature Toggles (compile-time) =====
// #define USE_SKINNING 1

// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;
layout(location = 1) in vec3 a_NormalOS;   // optional
layout(location = 2) in vec2 a_UV0;        // optional
// layout(location = 3) in vec4 a_TangentOS; // optional

// ===== Vertex Outputs (to Fragment Shader) =====
out VS_OUT {
    vec2 uv;
    vec3 normalWS;
    vec3 posWS;
} vs_out;

// ===== Uniform Blocks (optional, keep bindings stable) =====
layout(std140, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Proj;
    vec4 u_CameraPosWS; // xyz + padding
};

layout(std140, binding = 1) uniform ObjectUBO {
    mat4 u_Model;
    mat4 u_ModelInvT; // inverse-transpose for normals
};

// ===== Helpers =====
vec3 transformNormalWS(vec3 nOS)
{
    return normalize((u_ModelInvT * vec4(nOS, 0.0)).xyz);
}

void main()
{
    vec4 posWS = u_Model * vec4(a_PositionOS, 1.0);

    vs_out.posWS    = posWS.xyz;
    vs_out.normalWS = transformNormalWS(a_NormalOS);
    vs_out.uv       = a_UV0;

    gl_Position = u_Proj * u_View * posWS; // standard pipeline
}

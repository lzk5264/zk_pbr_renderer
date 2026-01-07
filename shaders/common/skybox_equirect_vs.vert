#version 460 core

// ============================================================================
// - Naming: a_* (attributes), u_* (uniforms), vs_out.* (varyings)
// - Spaces: OS (object), WS (world), VS (view), CS (clip)
// ============================================================================

// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;

// ===== Vertex Outputs (to Fragment Shader) =====
out VS_OUT {
    vec3 posOS;
} vs_out;

// ===== Uniform Blocks (optional, keep bindings stable) =====
layout(std140, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Proj;
};

// helper

void main()
{
    vs_out.posOS = a_PositionOS;
    mat4 viewNoTrans = mat4(mat3(u_View));
    vec4 pos = u_Proj * viewNoTrans * vec4(a_PositionOS, 1.0);
    gl_Position = pos.xyww;
}

#version 460 core

// ============================================================================
// Shadow Depth Pass - 顶点着色器
// 仅做 MVP 变换，输出深度值到 depth buffer
// ============================================================================

// ===== Vertex Inputs =====
// 只需要 POSITION，其他属性在 depth pass 中无用
// 但 VAO 布局与 PBRVertex 一致（12 floats/vertex），所以声明全部属性
layout(location = 0) in vec3 a_PositionOS;
layout(location = 1) in vec3 a_NormalOS;
layout(location = 2) in vec2 a_UV0;
layout(location = 3) in vec4 a_TangentOS;

// ===== Uniform Blocks =====
layout(std140, binding = 1) uniform ObjectUBO {
    mat4 u_Model;
    mat4 u_ModelInvT;
};

layout(std140, binding = 2) uniform LightingUBO {
    vec4 u_LightDir;
    vec4 u_LightColor;
    mat4 u_LightSpaceMatrix;
};

void main()
{
    gl_Position = u_LightSpaceMatrix * u_Model * vec4(a_PositionOS, 1.0);
}

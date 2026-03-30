#version 460 core

// ===== Vertex Inputs =====
// 布局与 layouts::PBRVertex() 严格对应（12 floats/vertex）。
layout(location = 0) in vec3 a_PositionOS;
layout(location = 1) in vec3 a_NormalOS;
layout(location = 2) in vec2 a_UV0;
layout(location = 3) in vec4 a_TangentOS;  // xyz = tangent, w = handedness (±1)

// ===== Vertex Outputs (to Fragment Shader) =====
out V2F {
    vec3 posWS;
    vec3 normalWS;
    vec4 tangentWS;
    vec2 uv0;
    vec4 posLS;       // light-space position (shadow mapping)
} v_Out;


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

layout(std140, binding = 2) uniform LightingUBO {
    vec4 u_LightDir;
    vec4 u_LightColor;
    mat4 u_LightSpaceMatrix;
};

// ===== Helpers =====
vec3 transformNormalWS(vec3 nOS)
{
    return normalize((u_ModelInvT * vec4(nOS, 0.0)).xyz);
}

void main()
{
    vec4 posWS = u_Model * vec4(a_PositionOS, 1.0);

    v_Out.posWS     = posWS.xyz;
    v_Out.normalWS  = transformNormalWS(a_NormalOS);
    // NOTE: w 分量存的是 handedness (±1)，不能参与矩阵变换
    v_Out.tangentWS = vec4(mat3(u_Model) * a_TangentOS.xyz, a_TangentOS.w);
    v_Out.uv0       = a_UV0;
    v_Out.posLS     = u_LightSpaceMatrix * posWS;

    gl_Position = u_Proj * u_View * posWS; // standard pipeline
}

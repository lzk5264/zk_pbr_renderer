#version 460 core

// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;
layout(location = 1) in vec3 a_NormalOS;   
layout(location = 2) in vec2 a_UV0;        
layout(location = 3) in vec2 a_UV1;
layout(location = 4) in vec4 a_TangentOS;

// ===== Vertex Outputs (to Fragment Shader) =====
out V2F {
    vec3 posWS;
    vec3 normalWS;
    vec4 tangentWS;
    vec2 uv0;
    vec2 uv1;
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
    v_Out.tangentWS = u_Model * a_TangentOS;
    v_Out.uv0       = a_UV0;
    v_Out.uv1       = a_UV1;

    gl_Position = u_Proj * u_View * posWS; // standard pipeline
}

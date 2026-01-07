#version 460 core

// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;

// ===== Uniforms =====
layout(std140, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Proj;
};


layout(std140, binding = 1) uniform ObjectUBO {
    mat4 u_Model;
};

// ===== Vertex Outputs (to Fragment Shader) =====
out VS_OUT {
    vec2 uv;
} vs_out;

void main()
{
    gl_Position = u_Proj * u_View * u_Model * vec4(a_PositionOS, 1.0);
}

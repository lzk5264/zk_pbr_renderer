#version 460 core


// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;

// ===== Vertex Outputs (to Fragment Shader) =====
out VS_OUT {
    vec3 uv;
} vs_out;

// ===== Uniform Blocks (optional, keep bindings stable) =====
layout(std140, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Proj;
};


void main()
{
    vs_out.uv = a_PositionOS;
    gl_Position = u_Proj * u_View * vec4(a_PositionOS, 1.0);
}

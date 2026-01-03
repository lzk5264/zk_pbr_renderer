#version 460 core

// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;

// ===== Uniforms =====
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// ===== Vertex Outputs (to Fragment Shader) =====
out VS_OUT {
    vec2 uv;
} vs_out;

void main()
{
    gl_Position = projection * view * model * vec4(a_PositionOS, 1.0);
}

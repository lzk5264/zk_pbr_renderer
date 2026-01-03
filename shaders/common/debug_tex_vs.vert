#version 460 core

// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;
layout(location = 1) in vec2 a_UV0;

// ===== Vertex Outputs (to Fragment Shader) =====
out VS_OUT {
    vec2 uv;
} vs_out;

void main()
{
    vs_out.uv = a_UV0;
    gl_Position = vec4(a_PositionOS, 1.0); // passthrough: OS == clip (debug use)
}

#version 460 core

// ===== Vertex Inputs =====
layout(location = 0) in vec3 a_PositionOS;

void main()
{
    gl_Position = vec4(a_PositionOS, 1.0); // standard pipeline
}

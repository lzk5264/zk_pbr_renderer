#version 460 core

layout(location = 0) in vec3 a_PositionOS;

out VS_OUT {
    vec3 uv;
} vs_out;


layout(std140, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Proj;
};


void main()
{
    vs_out.uv = a_PositionOS;
    gl_Position = u_Proj * u_View * vec4(a_PositionOS, 1.0);
}

#version 460 core

// IBL 预计算阶段的通用 Cubemap 捕获 VS
// 用于 Equirect→Cubemap、Irradiance 卷积、Prefiltered Env Map 等离屏渲染
// binding = 15（kInternal），避免与运行时 CameraUBO(binding=0) 冲突

layout(location = 0) in vec3 a_Position;
out vec3 v_LocalPos;

layout(std140, binding = 15) uniform CaptureMatrices 
{
    mat4 u_View;
    mat4 u_Projection;
};

void main() 
{
    v_LocalPos = a_Position;
    gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}
#version 460 core

// Equirectangular → Cubemap 转换（配合 capture_cubemap_vs.vert 使用）
// 将等距矩形 HDR 贴图采样到 Cubemap 的单个面上

in vec3 v_LocalPos;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_EquirectMap;

const float PI = 3.14159265359;

void main() 
{
    vec3 dir = normalize(v_LocalPos);
    
    // 笛卡尔坐标 → 球面坐标 → UV
    // phi ∈ [-π, π] → u ∈ [0, 1]，theta ∈ [-π/2, π/2] → v ∈ [0, 1]
    float phi = atan(dir.z, dir.x); // atan(z, x) 计算方位角 phi
    float theta = asin(dir.y); // asin(y) 计算仰角 theta
    vec2 uv = vec2(phi / (2.0 * PI) + 0.5, theta / PI + 0.5); // 球面坐标转 UV
    
    FragColor = vec4(texture(u_EquirectMap, uv).rgb, 1.0);
}
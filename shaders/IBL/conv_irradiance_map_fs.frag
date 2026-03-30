#version 460 core


// IBL Irradiance Map 卷积（配合 capture_cubemap_vs.vert 使用）
// 蒙特卡洛估计：E[Li] = 1/N * Σ Li(ωi) * cos(θi) / pdf(θi)
// 采用 cosine-weighted hemisphere sampling，pdf(θ) = cos(θ) / π
// cos(θ) / pdf(θ) = π，故简化为：Irradiance = π/N * Σ Li(ωi)

in vec3 v_LocalPos;

layout(location = 0) out vec4 o_Color;
layout(binding = 0) uniform samplerCube u_Cubemap;

layout(location = 0) uniform int u_Samplers; // 采样数


const float PI = 3.14159265359;
const float PiOver4 = 0.7853981633974483; // PI / 4
const float PiOver2 = 1.5707963267948966; // PI / 2


// Helper 函数，被 Hammersley() 调用
float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley 序列生成，其中 i 是样本索引，N 是总样本数
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N + 1), RadicalInverse_VdC(i));
} 

// pixar 正交基构造方法
// 输入 n 必须是单位向量
// 输出 b1, b2 与 n 构成正交基：{b1, b2, n}
void GetONB(in vec3 n, out vec3 b1, out vec3 b2)
{
    float s = mix(-1.0, 1.0, step(0.0, n.z)); // n.z>=0 -> 1, 否则 -1

    float a = -1.0 / (s + n.z);
    float b = n.x * n.y * a;

    b1 = vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
    b2 = vec3(b, s + n.y * n.y * a,  -n.y);
}


// 给定两个 [0, 1) 的随机数 u，返回一个在单位圆内的点
// 是 concentric mapping 的代码实现
vec2 SampleUniformDiskConcentric(vec2 u)
{
    vec2 uOffset = 2 * u - vec2(1.0f, 1.0f);

    if (uOffset.x == 0 && uOffset.y == 0)
        return vec2(0.0f, 0.0f);


    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y)) 
    {
        r = uOffset.x;
        theta = PiOver4 * (uOffset.y / uOffset.x);
    } 
    else 
    {
        r = uOffset.y;
        theta = PiOver2 - PiOver4 * (uOffset.x / uOffset.y);
    }
    return r * vec2(cos(theta), sin(theta));
}

// 给定两个 [0, 1) 的随机数 u，返回一个在半球内的单位向量
// 将 concentric mapping 生成的二维向量映射到半球
vec3 SampleCosineHemisphere(vec2 u)
{
    vec2 d = SampleUniformDiskConcentric(u);
    float z = sqrt(1 - d.x * d.x - d.y * d.y);
    return vec3(d.x, d.y, z);
}


void main()
{
    vec3 N = normalize(v_LocalPos);
    vec3 B1, B2;
    GetONB(N, B1, B2);


    vec3 sum = vec3(0.0);
    for (int i = 0; i < u_Samplers; i ++)
    {
        vec2 Xi = Hammersley(i, u_Samplers);
        vec3 L = SampleCosineHemisphere(Xi);
        L = normalize(L.x * B1 + L.y * B2 + L.z * N);

        vec3 Li = texture(u_Cubemap, L).rgb;
        sum += Li;
    }

    o_Color = vec4(PI * sum / float(u_Samplers), 1.0);
}

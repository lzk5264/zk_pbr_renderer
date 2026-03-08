#version 460 core

in VS_OUT {
    vec3 uv;
} fs_in;

layout(location = 0) out vec4 o_Color;
layout(binding = 0) uniform samplerCube u_Cubemap;

layout(location = 0) uniform int u_Samplers;


const float PI = 3.14159265359;
const float PiOver4 = 0.7853981633974483; // Π / 4
const float PiOver2 = 1.5707963267948966; // Π / 2


// Hammersley
float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N + 1), RadicalInverse_VdC(i));
} 


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

vec3 SampleCosineHemisphere(vec2 u)
{
    vec2 d = SampleUniformDiskConcentric(u);
    float z = sqrt(1 - d.x * d.x - d.y * d.y);
    return vec3(d.x, d.y, z);
}


void main()
{
    uvec2 pix = uvec2(gl_FragCoord.xy);
    uint frame = 0u; // 如果你有时间累积/降噪，就传进来；没有就保持 0

    vec3 N = normalize(fs_in.uv);
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

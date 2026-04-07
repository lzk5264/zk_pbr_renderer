#version 460 core

// ===== Fragment Inputs (from Vertex Shader) =====
in V2F {
    vec3 posWS;
    vec3 normalWS;
    vec4 tangentWS;   // xyz = tangent, w = handedness
    vec2 uv0;
    vec4 posLS;       // light-space position (shadow mapping)
} v_In;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources (keep bindings stable across shaders) =====
layout(binding = 0) uniform sampler2D u_AlbedoTex;
layout(binding = 1) uniform sampler2D u_NormalTex;
layout(binding = 2) uniform sampler2D u_MetallicRoughnessTex;
layout(binding = 3) uniform sampler2D u_AOTex;
layout(binding = 4) uniform sampler2D u_EmissiveTex;
layout(binding = 5) uniform samplerCube u_IrradianceMap;
layout(binding = 6) uniform samplerCube u_PrefilteredEnvMap;
layout(binding = 7) uniform sampler2D u_DFGLUT;
layout(binding = 8) uniform sampler2D u_ShadowMap;

// ===== Uniforms (small scalar params; use UBO if grows) =====
layout(location = 0) uniform vec4 u_BaseColor = vec4(1.0); 
layout(location = 1) uniform float u_MetallicFactor  = 1.0;  
layout(location = 2) uniform float u_RoughnessFactor = 1.0;  
layout(location = 3) uniform vec3 u_EmissiveFactor = vec3(1.0);

// ===== Uniform Blocks =====
layout(std140, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Proj;
    vec4 u_CameraPosWS;
};

layout(std140, binding = 2) uniform LightingUBO {
    vec4 u_LightDir;           // xyz = 指向光源方向 (normalized), w = padding
    vec4 u_LightColor;         // xyz = 颜色 × 强度, w = padding
    mat4 u_LightSpaceMatrix;   // 光源 VP 矩阵 (shadow mapping)
};

const float PI    = 3.1415926;
const float invPI = 0.31830988618;

// ===== Surface Data =====
struct SurfaceData {
    vec3  baseColor;
    vec3  diffuseColor;
    vec3  f0;
    float f90;
    float perceptualRoughness;
    float roughness;           // perceptualRoughness²
    vec3  normalWS;
    vec3  viewDirWS;
    float NdotV;
};

// ===== BRDF Functions =====

// Fresnel-Schlick 近似
vec3 F_Schlick(float VdotH, vec3 f0)
{
    float x  = 1.0 - VdotH;
    float x2 = x * x;
    float x4 = x2 * x2;
    float x5 = x4 * x;
    return f0 + (1.0 - f0) * x5;
}

// GGX / Trowbridge-Reitz 法线分布函数
float D_GGX(float NdotH, float a2)
{
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Smith GGX Height-Correlated Visibility（已包含 BRDF 分母 4*NdotV*NdotL）
// 非 sqrt 优化版本
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2)
{
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / max(GGXV + GGXL, 1e-7);
}

// ===== Shadow Calculation =====
float calcShadow(vec4 posLS, vec3 N, vec3 L)
{
    // 透视除法
    vec3 projCoords = posLS.xyz / posLS.w;
    projCoords = projCoords * 0.5 + 0.5;

    // 超出光源视锥的区域不产生阴影
    if (projCoords.z > 1.0)
        return 1.0;

    float currentDepth = projCoords.z;

    // 自适应 bias：表面越倾斜，bias 越大
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);

    // PCF 3x3
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}

// ===== Surface Construction =====
SurfaceData buildSurface()
{
    SurfaceData s;

    // Base color
    s.baseColor = texture(u_AlbedoTex, v_In.uv0).rgb * u_BaseColor.rgb;

    // Metallic / roughness
    vec4  mrSample = texture(u_MetallicRoughnessTex, v_In.uv0);
    float metallic            = mrSample.b * u_MetallicFactor;
    s.perceptualRoughness     = mrSample.g * u_RoughnessFactor;
    s.roughness               = s.perceptualRoughness * s.perceptualRoughness;

    // Derived material params
    s.diffuseColor = (1.0 - metallic) * s.baseColor;
    s.f0  = mix(vec3(0.04), s.baseColor, metallic);
    s.f90 = 1.0;

    // Normal mapping: TBN -> world space
    vec3 N = normalize(v_In.normalWS);
    vec3 T = normalize(v_In.tangentWS.xyz);
    T = normalize(T - dot(T, N) * N);         // Gram-Schmidt orthogonalization
    vec3 B = v_In.tangentWS.w * cross(N, T);  // handedness
    mat3 TBN = mat3(T, B, N);
    vec3 normalTS = texture(u_NormalTex, v_In.uv0).xyz * 2.0 - 1.0;
    s.normalWS = normalize(TBN * normalTS);

    // View direction & NdotV
    s.viewDirWS = normalize(u_CameraPosWS.rgb - v_In.posWS);
    s.NdotV     = max(dot(s.normalWS, s.viewDirWS), 1e-4);

    return s;
}

// ===== Direct Lighting (Cook-Torrance + Lambert) =====
vec3 evaluateDirectLight(SurfaceData s)
{
    vec3  L     = normalize(u_LightDir.xyz);
    vec3  H     = normalize(s.viewDirWS + L);
    float NdotL = max(dot(s.normalWS, L), 0.0);
    float NdotH = max(dot(s.normalWS, H), 0.0);
    float VdotH = max(dot(s.viewDirWS, H), 0.0);

    if (NdotL <= 0.0) return vec3(0.0);

    float a2 = s.roughness * s.roughness; // alpha² for GGX

    // Cook-Torrance specular BRDF
    float D = D_GGX(NdotH, a2);
    float V = V_SmithGGXCorrelated(s.NdotV, NdotL, a2);
    vec3  F = F_Schlick(VdotH, s.f0);

    vec3 specular = D * V * F;

    // Lambert diffuse（能量守恒：(1 - F) 部分给漫反射）
    vec3 diffuse = (1.0 - F) * s.diffuseColor * invPI;

    // Shadow
    float shadow = calcShadow(v_In.posLS, s.normalWS, L);

    return (diffuse + specular) * u_LightColor.rgb * NdotL * shadow;
}

// ===== IBL Evaluation =====
vec3 evaluateIBL(SurfaceData s)
{
    vec3 fresnel = F_Schlick(s.NdotV, s.f0);

    // Diffuse: irradiance map stores PI * mean(Li * NdotL)
    // Lambert BRDF = albedo/PI  =>  Lo = (albedo/PI) * irradiance
    vec3 irradiance = texture(u_IrradianceMap, s.normalWS).rgb;
    vec3 diffuseIBL = (1.0 - fresnel) * s.diffuseColor * invPI * irradiance;

    // Specular: split-sum  Lo = LD * (F0*DFG1 + F90*DFG2)
    vec3  R      = reflect(-s.viewDirWS, s.normalWS);
    float maxMip = float(textureQueryLevels(u_PrefilteredEnvMap) - 1);
    vec3  ld     = textureLod(u_PrefilteredEnvMap, R, s.perceptualRoughness * maxMip).rgb;
    vec2  dfg    = texture(u_DFGLUT, vec2(s.NdotV, s.perceptualRoughness)).rg;
    vec3  specularIBL = (s.f0 * dfg.x + s.f90 * dfg.y) * ld;

    return diffuseIBL + specularIBL;
}

// ===== Main =====
void main()
{
    SurfaceData s = buildSurface();

    float ao       = texture(u_AOTex,       v_In.uv0).r;
    vec3  emissive = texture(u_EmissiveTex, v_In.uv0).rgb * u_EmissiveFactor;

    vec3 direct  = evaluateDirectLight(s);
    vec3 ambient = evaluateIBL(s) * ao;

    vec3 color = direct + ambient + emissive;
    o_Color = vec4(color, 1.0);
}

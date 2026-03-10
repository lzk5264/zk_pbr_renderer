#version 460 core

// ===== Fragment Inputs (from Vertex Shader) =====
in V2F {
    vec3 posWS;
    vec3 normalWS;
    vec4 tangentWS;   // xyz = tangent, w = handedness
    vec2 uv0;
    vec2 uv1;
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

// ===== Uniforms (small scalar params; use UBO if grows) =====
layout(location = 0) uniform vec4 u_BaseColor = vec4(1.0); 
layout(location = 1) uniform float u_MetallicFactor  = 1.0;  
layout(location = 2) uniform float u_RoughnessFactor = 1.0;  
layout(location = 3) uniform vec3 u_EmissiveFactor = vec3(1.0);


layout(std140, binding = 0) uniform CameraUBO {
    mat4 u_View;
    mat4 u_Proj;
    vec4 u_CameraPosWS;
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
    vec3  normalWS;
    vec3  viewDirWS;
    float NdotV;
};

// ===== Helpers =====
vec3 fresnelSchlick(float NdotV, vec3 f0)
{
    float x  = 1.0 - NdotV;
    float x2 = x * x;
    float x4 = x2 * x2;
    float x5 = x4 * x;
    return f0 + (1.0 - f0) * x5;
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

// ===== IBL Evaluation =====
vec3 evaluateIBL(SurfaceData s)
{
    vec3 fresnel = fresnelSchlick(s.NdotV, s.f0);

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

    float ao      = texture(u_AOTex,        v_In.uv0).r;
    vec3  emissive = texture(u_EmissiveTex, v_In.uv0).rgb * u_EmissiveFactor;

    vec3 color = evaluateIBL(s) * ao + emissive;
    o_Color = vec4(color, 1.0);
}

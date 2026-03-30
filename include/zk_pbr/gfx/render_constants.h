#pragma once

#include <glm/glm.hpp>

namespace zk_pbr::gfx
{

    // ============================================================================
    // texture slot（全局约定）
    // ============================================================================

    namespace texture_slot
    {
        // 材质级 (Material::Bind() 负责)
        constexpr unsigned int kAlbedo = 0;
        constexpr unsigned int kNormal = 1;
        constexpr unsigned int kMetallicRoughness = 2;
        constexpr unsigned int kAO = 3;
        constexpr unsigned int kEmissive = 4;

        // 场景级 (SceneEnvironment::Bind() 负责，每帧绑一次)
        constexpr unsigned int kIrradianceMap = 5;
        constexpr unsigned int kPrefilteredEnvMap = 6;
        constexpr unsigned int kDFGLUT = 7;

        // 阴影级 (Shadow pass 产出，PBR shader 消费)
        constexpr unsigned int kShadowMap = 8;
    }

    // ============================================================================
    // UBO Binding Points（全局约定）
    // ============================================================================
    namespace ubo_binding
    {
        constexpr unsigned int kCamera = 0;   // 相机矩阵（view, projection）
        constexpr unsigned int kObject = 1;   // 对象变换（model matrix）
        constexpr unsigned int kLighting = 2; // 光照参数
        constexpr unsigned int kMaterial = 3; // 材质参数

        // 内部/临时用途（避免与常用 binding point 冲突）
        constexpr unsigned int kInternal = 15;
    }

    // 相机 UBO 数据结构
    // 对应 shader 中的:
    //   layout(std140, binding = 0) uniform CameraUBO {
    //       mat4 u_View;
    //       mat4 u_Proj;
    //       vec4 u_CameraPosWS;
    //   };
    struct CameraUBOData
    {
        glm::mat4 view;          // offset 0,   size 64, align 16
        glm::mat4 projection;    // offset 64,  size 64, align 16
        glm::vec4 camera_pos_ws; // offset 128, size 16, align 16（xyz = 位置，w = std140 padding，shader 仅读 .xyz）

        // 通过此辅助方法赋值，避免调用侧手动构造 vec4。
        void SetCameraPos(const glm::vec3 &pos) { camera_pos_ws = glm::vec4(pos, 0.0f); }
    };
    static_assert(sizeof(CameraUBOData) == 144, "CameraUBOData size mismatch (expected 144 bytes for std140)");

    // ============================================================================
    // 对象变换 UBO 数据结构
    // 对应 shader 中的:
    //   layout(std140, binding = 1) uniform ObjectUBO {
    //       mat4 u_Model;
    //   };
    // ============================================================================
    struct ObjectUBOData
    {
        glm::mat4 model;       // offset 0, size 64, align 16
        glm::mat4 model_inv_t; // offset 64, size 64, align 16
    };
    static_assert(sizeof(ObjectUBOData) == 128, "ObjectUBOData size mismatch (expected 128 bytes for std140)");

    // ============================================================================
    // Material Uniform Locations（explicit layout location，与 pbr_shading_fs.frag 对应）
    // 修改此处时，必须同步修改 shader 中的 layout(location = N)
    // ============================================================================
    namespace uniform_location
    {
        constexpr int kBaseColorFactor = 0; // vec4  u_BaseColor
        constexpr int kMetallicFactor = 1;  // float u_MetallicFactor
        constexpr int kRoughnessFactor = 2; // float u_RoughnessFactor
        constexpr int kEmissiveFactor = 3;  // vec3  u_EmissiveFactor
    }

    // ============================================================================
    // 光照 UBO 数据结构
    // 对应 shader 中的:
    //   layout(std140, binding = 2) uniform LightingUBO {
    //       vec4  u_LightDir;          // xyz = 指向光源方向, w = padding
    //       vec4  u_LightColor;        // xyz = 颜色 × 强度, w = padding
    //       mat4  u_LightSpaceMatrix;  // 光源 VP 矩阵 (shadow mapping)
    //   };
    // ============================================================================
    struct LightingUBOData
    {
        glm::vec4 light_dir;          // offset 0,  size 16, align 16
        glm::vec4 light_color;        // offset 16, size 16, align 16
        glm::mat4 light_space_matrix; // offset 32, size 64, align 16
    };
    static_assert(sizeof(LightingUBOData) == 96, "LightingUBOData size mismatch (expected 96 bytes for std140)");

} // namespace zk_pbr::gfx

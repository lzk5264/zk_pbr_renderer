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

    // ============================================================================
    // 相机 UBO 数据结构
    // 对应 shader 中的:
    //   layout(std140, binding = 0) uniform CameraUBO {
    //       mat4 u_View;
    //       mat4 u_Proj;
    //       vec4 u_CameraPosWS;
    //   };
    // ============================================================================
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

} // namespace zk_pbr::gfx

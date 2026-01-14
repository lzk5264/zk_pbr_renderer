#pragma once

#include <glm/glm.hpp>

namespace zk_pbr::gfx
{

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
    //   };
    // ============================================================================
    struct CameraUBOData
    {
        glm::mat4 view;       // offset 0,  size 64, align 16
        glm::mat4 projection; // offset 64, size 64, align 16
    };
    static_assert(sizeof(CameraUBOData) == 128, "CameraUBOData size mismatch (expected 128 bytes for std140)");

    // ============================================================================
    // 对象变换 UBO 数据结构
    // 对应 shader 中的:
    //   layout(std140, binding = 1) uniform ObjectUBO {
    //       mat4 u_Model;
    //   };
    // ============================================================================
    struct ObjectUBOData
    {
        glm::mat4 model; // offset 0, size 64, align 16
    };
    static_assert(sizeof(ObjectUBOData) == 64, "ObjectUBOData size mismatch (expected 64 bytes for std140)");

} // namespace zk_pbr::gfx

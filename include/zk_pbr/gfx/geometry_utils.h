#pragma once

#include <zk_pbr/gfx/mesh.h>

namespace zk_pbr::gfx
{

    // 几何体生成工具（PBR 专用版本）
    // 与 PrimitiveFactory 的区别：包含 tangent 数据用于法线贴图
    // 如果不需要法线贴图，使用 PrimitiveFactory 更轻量
    namespace GeometryUtils
    {

        // 生成 UV 球体（经纬度划分）
        // @param radius: 球体半径
        // @param segments: 经线和纬线的段数（越大越平滑，建议 32-64）
        // @return: Mesh 对象，包含位置、法线、UV、切线
        // 顶点布局：position(vec3), normal(vec3), uv(vec2), tangent(vec3)
        [[nodiscard]] Mesh CreateSphere(float radius = 1.0f, int segments = 64);

        // 生成平面（用于地面/测试）
        // @param size: 平面大小
        // @param subdivisions: 细分数（影响顶点密度）
        // @return: Mesh 对象，法线朝上 (0, 1, 0)
        [[nodiscard]] Mesh CreatePlane(float size = 10.0f, int subdivisions = 1);

    } // namespace GeometryUtils

} // namespace zk_pbr::gfx

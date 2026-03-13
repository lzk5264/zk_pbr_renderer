#pragma once
#include <memory>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <zk_pbr/gfx/texture2d.h>

namespace zk_pbr::gfx
{
    // 材质纹理缺失时的兜底资源。
    //
    // 这些纹理通常是 1x1 程序纹理，不依赖外部文件：
    //   - White:      albedo / ao 回退
    //   - FlatNormal: 法线回退，值为 (0.5, 0.5, 1.0)
    //   - Black:      emissive 回退
    //   - DefaultMR:  metallic=0, roughness=1
    //
    // 设计意图：即使模型缺失贴图，也保证 shader 采样合法，避免出现未定义结果。
    // 也因为这个兜底设计，就不设计Material异常相关的内容了
    namespace DefaultTextures
    {
        const std::shared_ptr<Texture2D> &White();      // albedo / ao 回退
        const std::shared_ptr<Texture2D> &FlatNormal(); // (0.5, 0.5, 1.0)
        const std::shared_ptr<Texture2D> &Black();      // emissive 回退
        const std::shared_ptr<Texture2D> &DefaultMR();  // metallic=0, roughness=1
    }

    // Mesh 的 PBR 材质参数与纹理资源。
    //
    // 该类管理 5 张材质贴图：
    //   1. albedo              - 基础色
    //   2. normal              - 切线空间法线
    //   3. metallic_roughness  - B 通道 metallic, G 通道 roughness
    //   4. ao                  - 环境光遮蔽
    //   5. emissive            - 自发光
    //
    // 使用场景：
    //   - 模型导入时填充贴图与因子参数
    //   - 渲染 draw call 前调用 Bind（随后 shader 采样这些纹理）
    class Material
    {
    public:
        // 纹理资源（可为空，运行时由兜底纹理补齐）。
        std::shared_ptr<Texture2D> albedo;
        std::shared_ptr<Texture2D> normal;
        std::shared_ptr<Texture2D> metallic_roughness;
        std::shared_ptr<Texture2D> ao;
        std::shared_ptr<Texture2D> emissive;

        // 标量/向量因子，用于在 shader 中与纹理采样值组合。
        glm::vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
        float metallic_factor{1.0f};
        float roughness_factor{1.0f};
        glm::vec3 emissive_factor{0.0f, 0.0f, 0.0f};

        // 将材质贴图与 factor 因子绑定到当前 GL context。
        //
        // 纹理绑定（binding = N，对应 render_constants::texture_slot）：
        //   - slot 0: albedo, slot 1: normal, slot 2: metallic_roughness
        //   - slot 3: ao,     slot 4: emissive
        //
        // 因子绑定（layout location = N，对应 render_constants::uniform_location）：
        //   - location 0: base_color_factor, 1: metallic_factor
        //   - location 2: roughness_factor,  3: emissive_factor
        //
        // 前置条件：当前线程存在有效 GL context，且激活的 shader 使用对应的绑定协议
        // 注意：空贴图自动回退到 DefaultTextures，不会产生 UB
        void Bind() const;
    };
}
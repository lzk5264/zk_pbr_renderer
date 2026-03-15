#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <zk_pbr/gfx/mesh.h>
#include <zk_pbr/gfx/material.h>
#include <zk_pbr/gfx/shader.h>

struct cgltf_node; // 前向声明

namespace zk_pbr::gfx
{
    // 模型加载/解析失败时抛出。
    //
    // 消息格式通常为：
    //   "Failed to load '<path>': <原因>"
    // 其中原因可能包含下层异常（TextureException、MeshException 等）的 what()。
    struct ModelException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Model 的组成单位，包含一个 GPU 网格与其对应的 PBR 材质。
    // glTF 中一个 Mesh 节点可含多个 Primitive，每个 Primitive 拥有
    // 独立的顶点缓冲与材质，因此以 MeshPrimitive 作为最小渲染单元。
    struct MeshPrimitive
    {
        Mesh mesh;
        Material material;
        glm::mat4 model_matrix;
    };

    // 渲染指令，包含一次 draw call 的必要信息.
    // 未来可扩展：state sorting（减少状态切换）、depth sorting（透明物体排序）等.
    struct DrawCommand
    {
        const Mesh *mesh;
        const Material *material;
        glm::mat4 model_matrix;
        glm::mat4 model_inv_t;  // 预计算的法线变换矩阵（transpose(inverse(model))）
    };

    // 表示一个完整的 glTF 2.0 模型，由若干 MeshPrimitive 构成。
    //
    // 使用场景：
    //   auto model = Model::LoadFromGLTF("./resources/models/foo.gltf");
    //   // 渲染循环中：
    //   pbr_shader.Use();
    //   scene_env.BindIBL();
    //   auto drawCommands = model.GetDrawCommands();
    //   ...遍历 drawCommands 渲染
    class Model
    {
    public:
        // 从 glTF 2.0 文件（.gltf）加载模型并上传至 GPU。
        // 暂不支持 .glb（内嵌纹理需通过 buffer_view 读取，尚未实现）。
        //
        // 解析场景中所有网格 Primitive，提取 POSITION / NORMAL /
        // TEXCOORD_0 / TANGENT 顶点属性，同时加载对应的 PBR 材质贴图
        // （albedo、normal、metallic_roughness、ao、emissive）。
        //
        // path: 模型文件路径，支持相对路径与绝对路径。
        // 若文件不存在或格式不合法，抛出 ModelException。
        static Model LoadFromGLTF(const std::string &path);

        // 将模型的所有 MeshPrimitive 转换为 DrawCommand 返回
        std::vector<DrawCommand> GetDrawCommands() const;

    private:
        // 被 LoadFromGLTF 函数调用的内部辅助函数，用于递归遍历 scene graph 的节点
        // 处理父子节点变换关系，将 cgltf_node 及其 mesh 数据转换为 MeshPrimitive。
        // base_path 用于解析模型引用的外部贴图文件路径（如 .gltf 文件所在目录）。
        static void TraverseNode(const cgltf_node *node, Model &model, glm::mat4 world_matrix, const std::string &base_path);

        std::vector<MeshPrimitive> primitives_;
    };
} // namespace zk_pbr::gfx
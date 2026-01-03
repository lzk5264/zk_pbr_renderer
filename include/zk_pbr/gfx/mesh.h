#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include <zk_pbr/gfx/vertex_layout.h>

namespace zk_pbr::gfx
{

    // Mesh 异常类
    struct MeshException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Mesh 类：封装 VAO/VBO/EBO（RAII）
    class Mesh
    {
    public:
        Mesh() = default;

        // 构造：仅顶点数据（无索引）
        // throws MeshException 如果参数无效或 OpenGL 操作失败
        Mesh(const void *vertices, size_t vertex_count, const VertexLayout &layout, GLenum usage = GL_STATIC_DRAW);

        // 构造：顶点数据 + 索引数据
        // throws MeshException 如果参数无效或 OpenGL 操作失败
        Mesh(const void *vertices, size_t vertex_count,
             const unsigned int *indices, size_t index_count,
             const VertexLayout &layout, GLenum usage = GL_STATIC_DRAW);

        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;

        Mesh(Mesh &&other) noexcept;
        Mesh &operator=(Mesh &&other) noexcept;

        ~Mesh() noexcept;

        // 绘制网格
        void Draw(GLenum mode = GL_TRIANGLES) const noexcept;

        // 更新顶点数据（带边界检查）
        // throws MeshException 如果越界
        void UpdateVertexData(const void *vertices, size_t vertex_count, size_t offset = 0);

        // 更新索引数据（带边界检查）
        // throws MeshException 如果越界或 EBO 不存在
        void UpdateIndexData(const unsigned int *indices, size_t index_count, size_t offset = 0);

        [[nodiscard]] GLuint GetVAO() const noexcept { return vao_; }
        [[nodiscard]] GLuint GetVBO() const noexcept { return vbo_; }
        [[nodiscard]] GLuint GetEBO() const noexcept { return ebo_; }

        [[nodiscard]] size_t GetVertexCount() const noexcept { return vertex_count_; }
        [[nodiscard]] size_t GetIndexCount() const noexcept { return index_count_; }
        [[nodiscard]] size_t GetVertexStride() const noexcept { return vertex_stride_; }
        [[nodiscard]] bool HasIndices() const noexcept { return ebo_ != 0; }
        [[nodiscard]] bool IsValid() const noexcept { return vao_ != 0 && vbo_ != 0; }

    private:
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        GLuint ebo_ = 0;

        size_t vertex_count_ = 0;
        size_t index_count_ = 0;
        size_t vertex_stride_ = 0;

        void SetupMesh(const void *vertices, size_t vertex_count,
                       const unsigned int *indices, size_t index_count,
                       const VertexLayout &layout, GLenum usage);

        void Cleanup() noexcept;
    };

    // 基本图元工厂
    class PrimitiveFactory
    {
    public:
        // 创建三角形（仅位置）
        static Mesh CreateTriangle();

        // 创建四边形（位置 + 纹理坐标）
        static Mesh CreateQuad();

        // 创建立方体（位置 + 法线 + 纹理坐标）
        static Mesh CreateCube();

        // 创建球体（位置 + 法线 + 纹理坐标）
        static Mesh CreateSphere(unsigned int segments = 64, unsigned int rings = 32);

        // 创建平面（位置 + 法线 + 纹理坐标）
        static Mesh CreatePlane(float width = 2.0f, float height = 2.0f,
                                unsigned int width_segments = 1, unsigned int height_segments = 1);
    };

} // namespace zk_pbr::gfx

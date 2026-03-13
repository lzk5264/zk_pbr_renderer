#pragma once

#include <stdexcept>
#include <vector>

#include <glad/glad.h>

#include <zk_pbr/gfx/vertex_layout.h>

namespace zk_pbr::gfx
{

    struct MeshException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Mesh 类：封装 VAO/VBO/EBO
    class Mesh
    {
    public:
        Mesh() = default;

        // 构造：顶点 + 索引
        Mesh(const void *vertices, size_t vertex_count,
             const unsigned int *indices, size_t index_count,
             const VertexLayout &layout, GLenum usage = GL_STATIC_DRAW);

        // 构造：仅顶点
        Mesh(const void *vertices, size_t vertex_count,
             const VertexLayout &layout, GLenum usage = GL_STATIC_DRAW);

        ~Mesh();

        // 禁止拷贝，允许移动
        Mesh(const Mesh &) = delete;
        Mesh &operator=(const Mesh &) = delete;
        Mesh(Mesh &&other) noexcept;
        Mesh &operator=(Mesh &&other) noexcept;

        void Draw(GLenum mode = GL_TRIANGLES) const;

        GLuint GetVAO() const { return vao_; }
        bool IsValid() const { return vao_ != 0; }

    private:
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        GLuint ebo_ = 0;
        size_t vertex_count_ = 0;
        size_t index_count_ = 0;
        size_t vertex_stride_ = 0;

        void Cleanup();
    };

    // 基本图元工厂 (IBL 需要的)
    class PrimitiveFactory
    {
    public:
        // 创建立方体 (用于天空盒、Cubemap 渲染)
        static Mesh CreateCube();

        // 创建全屏四边形 (用于后处理)
        static Mesh CreateQuad();
    };

} // namespace zk_pbr::gfx

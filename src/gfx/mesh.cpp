#include <zk_pbr/gfx/mesh.h>

namespace zk_pbr::gfx
{

    // ========== Mesh 实现 ==========

    Mesh::Mesh(const void *vertices, size_t vertex_count,
               const unsigned int *indices, size_t index_count,
               const VertexLayout &layout, GLenum usage)
        : vertex_count_(vertex_count), index_count_(index_count), vertex_stride_(layout.GetStride())
    {
        if (!vertices || vertex_count == 0)
        {
            throw MeshException("Invalid vertex data");
        }

        // 创建 VAO
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        // 创建 VBO
        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertex_count * vertex_stride_),
                     vertices, usage);

        // 应用顶点布局
        layout.Apply();

        // 创建 EBO（如果有索引）
        if (indices && index_count > 0)
        {
            glGenBuffers(1, &ebo_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(index_count * sizeof(unsigned int)),
                         indices, usage);
        }

        glBindVertexArray(0);
    }

    Mesh::Mesh(const void *vertices, size_t vertex_count,
               const VertexLayout &layout, GLenum usage)
        : Mesh(vertices, vertex_count, nullptr, 0, layout, usage)
    {
    }

    Mesh::~Mesh()
    {
        Cleanup();
    }

    Mesh::Mesh(Mesh &&other) noexcept
        : vao_(other.vao_), vbo_(other.vbo_), ebo_(other.ebo_),
          vertex_count_(other.vertex_count_), index_count_(other.index_count_),
          vertex_stride_(other.vertex_stride_)
    {
        other.vao_ = 0;
        other.vbo_ = 0;
        other.ebo_ = 0;
    }

    Mesh &Mesh::operator=(Mesh &&other) noexcept
    {
        if (this != &other)
        {
            Cleanup();

            vao_ = other.vao_;
            vbo_ = other.vbo_;
            ebo_ = other.ebo_;
            vertex_count_ = other.vertex_count_;
            index_count_ = other.index_count_;
            vertex_stride_ = other.vertex_stride_;

            other.vao_ = 0;
            other.vbo_ = 0;
            other.ebo_ = 0;
        }
        return *this;
    }

    void Mesh::Draw(GLenum mode) const
    {
        if (vao_ == 0)
            return;

        glBindVertexArray(vao_);

        if (index_count_ > 0)
        {
            glDrawElements(mode, static_cast<GLsizei>(index_count_), GL_UNSIGNED_INT, nullptr);
        }
        else
        {
            glDrawArrays(mode, 0, static_cast<GLsizei>(vertex_count_));
        }

        glBindVertexArray(0);
    }

    void Mesh::Cleanup()
    {
        if (vao_)
        {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (vbo_)
        {
            glDeleteBuffers(1, &vbo_);
            vbo_ = 0;
        }
        if (ebo_)
        {
            glDeleteBuffers(1, &ebo_);
            ebo_ = 0;
        }
    }

    // ========== PrimitiveFactory 实现 ==========

    Mesh PrimitiveFactory::CreateCube()
    {
        // 仅位置 (用于 Cubemap 渲染，不需要法线和 UV)
        float vertices[] = {
            // 后面
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,
            // 前面
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
            // 左面
            -1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            // 右面
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, 1.0f,
            // 下面
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,
            // 上面
            -1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f};

        unsigned int indices[] = {
            0, 1, 2, 2, 3, 0,       // 后
            4, 5, 6, 6, 7, 4,       // 前
            8, 9, 10, 10, 11, 8,    // 左
            12, 13, 14, 14, 15, 12, // 右
            16, 17, 18, 18, 19, 16, // 下
            20, 21, 22, 22, 23, 20  // 上
        };

        return Mesh(vertices, 24, indices, 36, layouts::Position3D());
    }

    Mesh PrimitiveFactory::CreateQuad()
    {
        // 全屏三角形 (更高效，只需 3 个顶点)
        float vertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
            3.0f, -1.0f, 2.0f, 0.0f,
            -1.0f, 3.0f, 0.0f, 2.0f};

        VertexLayout layout;
        layout.AddFloat(0, 2, 4 * sizeof(float), 0);                 // position
        layout.AddFloat(1, 2, 4 * sizeof(float), 2 * sizeof(float)); // texcoord

        return Mesh(vertices, 3, layout);
    }

} // namespace zk_pbr::gfx

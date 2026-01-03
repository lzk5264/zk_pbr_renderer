#include <zk_pbr/gfx/mesh.h>

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace zk_pbr::gfx
{

    // ========== Mesh 实现 ==========

    Mesh::Mesh(const void *vertices, size_t vertex_count, const VertexLayout &layout, GLenum usage)
    {
        SetupMesh(vertices, vertex_count, nullptr, 0, layout, usage);
    }

    Mesh::Mesh(const void *vertices, size_t vertex_count,
               const unsigned int *indices, size_t index_count,
               const VertexLayout &layout, GLenum usage)
    {
        SetupMesh(vertices, vertex_count, indices, index_count, layout, usage);
    }

    Mesh::Mesh(Mesh &&other) noexcept
        : vao_(std::exchange(other.vao_, 0)),
          vbo_(std::exchange(other.vbo_, 0)),
          ebo_(std::exchange(other.ebo_, 0)),
          vertex_count_(std::exchange(other.vertex_count_, 0)),
          index_count_(std::exchange(other.index_count_, 0)),
          vertex_stride_(std::exchange(other.vertex_stride_, 0))
    {
    }

    Mesh &Mesh::operator=(Mesh &&other) noexcept
    {
        if (this != &other)
        {
            Cleanup();

            vao_ = std::exchange(other.vao_, 0);
            vbo_ = std::exchange(other.vbo_, 0);
            ebo_ = std::exchange(other.ebo_, 0);
            vertex_count_ = std::exchange(other.vertex_count_, 0);
            index_count_ = std::exchange(other.index_count_, 0);
            vertex_stride_ = std::exchange(other.vertex_stride_, 0);
        }
        return *this;
    }

    Mesh::~Mesh() noexcept
    {
        Cleanup();
    }

    void Mesh::SetupMesh(const void *vertices, size_t vertex_count,
                         const unsigned int *indices, size_t index_count,
                         const VertexLayout &layout, GLenum usage)
    {
        // 参数验证
        if (!vertices || vertex_count == 0)
        {
            throw MeshException("Invalid vertex data: null pointer or zero count");
        }
        if (!layout.IsValid())
        {
            throw MeshException("Invalid vertex layout");
        }
        if (indices && index_count == 0)
        {
            throw MeshException("Invalid index data: non-null pointer with zero count");
        }

        vertex_count_ = vertex_count;
        index_count_ = index_count;
        vertex_stride_ = layout.GetStride();

        if (vertex_stride_ == 0)
        {
            throw MeshException("Vertex stride cannot be zero");
        }

        // 创建 VAO
        glGenVertexArrays(1, &vao_);
        if (vao_ == 0)
        {
            throw MeshException("Failed to generate VAO");
        }
        glBindVertexArray(vao_);

        // 创建并填充 VBO
        glGenBuffers(1, &vbo_);
        if (vbo_ == 0)
        {
            glDeleteVertexArrays(1, &vao_);
            throw MeshException("Failed to generate VBO");
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertex_count * vertex_stride_), vertices, usage);

        // 应用顶点布局
        layout.Apply();

        // 如果有索引数据，创建并填充 EBO
        if (indices && index_count > 0)
        {
            glGenBuffers(1, &ebo_);
            if (ebo_ == 0)
            {
                glDeleteBuffers(1, &vbo_);
                glDeleteVertexArrays(1, &vao_);
                throw MeshException("Failed to generate EBO");
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(index_count * sizeof(unsigned int)), indices, usage);
        }

        // 解绑
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void Mesh::Draw(GLenum mode) const noexcept
    {
        if (!IsValid())
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

    void Mesh::UpdateVertexData(const void *vertices, size_t vertex_count, size_t offset)
    {
        if (!vertices)
        {
            throw MeshException("UpdateVertexData: vertices pointer is null");
        }
        if (offset + vertex_count > vertex_count_)
        {
            throw MeshException("UpdateVertexData: offset + count exceeds buffer size");
        }
        if (vbo_ == 0)
        {
            throw MeshException("UpdateVertexData: VBO is not initialized");
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset * vertex_stride_),
                        static_cast<GLsizeiptr>(vertex_count * vertex_stride_), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void Mesh::UpdateIndexData(const unsigned int *indices, size_t index_count, size_t offset)
    {
        if (!indices)
        {
            throw MeshException("UpdateIndexData: indices pointer is null");
        }
        if (ebo_ == 0)
        {
            throw MeshException("UpdateIndexData: EBO is not initialized (mesh has no indices)");
        }
        if (offset + index_count > index_count_)
        {
            throw MeshException("UpdateIndexData: offset + count exceeds buffer size");
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(offset * sizeof(unsigned int)),
                        static_cast<GLsizeiptr>(index_count * sizeof(unsigned int)), indices);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void Mesh::Cleanup() noexcept
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

    Mesh PrimitiveFactory::CreateTriangle()
    {
        float vertices[] = {
            -0.5f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,
            0.0f, 0.5f, 0.0f};

        return Mesh(vertices, 3, layouts::Position3D());
    }

    Mesh PrimitiveFactory::CreateQuad()
    {
        // 位置 + 纹理坐标
        float vertices[] = {
            // 位置              // 纹理坐标
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
            -0.5f, 0.5f, 0.0f, 0.0f, 1.0f};

        unsigned int indices[] = {
            0, 1, 2,
            2, 3, 0};

        return Mesh(vertices, 4, indices, 6, layouts::Position3DTexCoord2D());
    }

    Mesh PrimitiveFactory::CreateCube()
    {
        // 位置 + 法线 + 纹理坐标
        float vertices[] = {
            // 后面
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,
            // 前面
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            // 左面
            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            // 右面
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            // 下面
            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            // 上面
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};

        unsigned int indices[] = {
            0, 1, 2, 2, 3, 0,       // 后面
            4, 5, 6, 6, 7, 4,       // 前面
            8, 9, 10, 10, 11, 8,    // 左面
            12, 13, 14, 14, 15, 12, // 右面
            16, 17, 18, 18, 19, 16, // 下面
            20, 21, 22, 22, 23, 20  // 上面
        };

        return Mesh(vertices, 24, indices, 36, layouts::Position3DNormal3DTexCoord2D());
    }

    Mesh PrimitiveFactory::CreateSphere(unsigned int segments, unsigned int rings)
    {
        // 预分配内存以减少重分配
        const size_t vertex_count = (rings + 1) * (segments + 1);
        const size_t index_count = rings * segments * 6;

        std::vector<float> vertices;
        vertices.reserve(vertex_count * 8); // 每个顶点 8 个 float

        std::vector<unsigned int> indices;
        indices.reserve(index_count);

        const float PI = glm::pi<float>();

        // 生成顶点
        for (unsigned int ring = 0; ring <= rings; ++ring)
        {
            float phi = PI * static_cast<float>(ring) / static_cast<float>(rings);
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);

            for (unsigned int seg = 0; seg <= segments; ++seg)
            {
                float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
                float sin_theta = std::sin(theta);
                float cos_theta = std::cos(theta);

                // 位置
                float x = sin_phi * cos_theta;
                float y = cos_phi;
                float z = sin_phi * sin_theta;

                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // 法线（球体法线即归一化的位置）
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // 纹理坐标
                vertices.push_back(static_cast<float>(seg) / static_cast<float>(segments));
                vertices.push_back(static_cast<float>(ring) / static_cast<float>(rings));
            }
        }

        // 生成索引
        for (unsigned int ring = 0; ring < rings; ++ring)
        {
            for (unsigned int seg = 0; seg < segments; ++seg)
            {
                unsigned int current = ring * (segments + 1) + seg;
                unsigned int next = current + segments + 1;

                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1);

                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(next + 1);
            }
        }

        return Mesh(vertices.data(), vertex_count, indices.data(), indices.size(),
                    layouts::Position3DNormal3DTexCoord2D());
    }

    Mesh PrimitiveFactory::CreatePlane(float width, float height,
                                       unsigned int width_segments, unsigned int height_segments)
    {
        // 预分配内存
        const size_t vertex_count = (width_segments + 1) * (height_segments + 1);
        const size_t index_count = width_segments * height_segments * 6;

        std::vector<float> vertices;
        vertices.reserve(vertex_count * 8);

        std::vector<unsigned int> indices;
        indices.reserve(index_count);

        float half_width = width * 0.5f;
        float half_height = height * 0.5f;

        // 生成顶点
        for (unsigned int y = 0; y <= height_segments; ++y)
        {
            for (unsigned int x = 0; x <= width_segments; ++x)
            {
                float fx = static_cast<float>(x) / static_cast<float>(width_segments);
                float fy = static_cast<float>(y) / static_cast<float>(height_segments);

                // 位置
                vertices.push_back(fx * width - half_width);
                vertices.push_back(0.0f);
                vertices.push_back(fy * height - half_height);

                // 法线（向上）
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);

                // 纹理坐标
                vertices.push_back(fx);
                vertices.push_back(fy);
            }
        }

        // 生成索引
        for (unsigned int y = 0; y < height_segments; ++y)
        {
            for (unsigned int x = 0; x < width_segments; ++x)
            {
                unsigned int current = y * (width_segments + 1) + x;
                unsigned int next = current + width_segments + 1;

                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1);

                indices.push_back(current + 1);
                indices.push_back(next);
                indices.push_back(next + 1);
            }
        }

        return Mesh(vertices.data(), vertex_count, indices.data(), indices.size(),
                    layouts::Position3DNormal3DTexCoord2D());
    }

} // namespace zk_pbr::gfx

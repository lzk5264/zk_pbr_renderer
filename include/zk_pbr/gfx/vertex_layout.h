#pragma once

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

namespace zk_pbr::gfx
{

    // 顶点属性描述
    struct VertexAttribute
    {
        GLuint location;      // attribute location
        GLint size;           // 分量数量 (1, 2, 3, 4)
        GLenum type;          // GL_FLOAT, GL_INT 等
        GLboolean normalized; // 是否归一化
        GLsizei stride;       // 步长（字节）
        size_t offset;        // 偏移量（字节）

        constexpr VertexAttribute(GLuint loc, GLint sz, GLenum tp, GLboolean norm, GLsizei str, size_t off) noexcept
            : location(loc), size(sz), type(tp), normalized(norm), stride(str), offset(off) {}

        // 验证属性有效性
        [[nodiscard]] bool IsValid() const noexcept
        {
            return size >= 1 && size <= 4 && stride >= 0;
        }
    };

    // 顶点布局描述器
    class VertexLayout
    {
    public:
        VertexLayout() = default;

        // 添加属性（带验证）
        void AddAttribute(GLuint location, GLint size, GLenum type, GLboolean normalized, GLsizei stride, size_t offset);

        // 快速添加浮点属性
        void AddFloat(GLuint location, GLint size, GLsizei stride, size_t offset);

        // 应用布局到当前绑定的 VAO
        void Apply() const;

        // 验证布局有效性
        [[nodiscard]] bool IsValid() const noexcept;

        [[nodiscard]] const std::vector<VertexAttribute> &GetAttributes() const noexcept { return attributes_; }
        [[nodiscard]] size_t GetStride() const noexcept;
        [[nodiscard]] bool IsEmpty() const noexcept { return attributes_.empty(); }

    private:
        std::vector<VertexAttribute> attributes_;
    };

    // 预定义的常用顶点布局
    namespace layouts
    {
        // 只有位置 (vec3)
        VertexLayout Position3D();

        // 位置 + 颜色 (vec3 + vec3)
        VertexLayout Position3DColor3D();

        // 位置 + 纹理坐标 (vec3 + vec2)
        VertexLayout Position3DTexCoord2D();

        // 位置 + 法线 + 纹理坐标 (vec3 + vec3 + vec2)
        VertexLayout Position3DNormal3DTexCoord2D();

        // 完整 PBR 顶点 (位置 + 法线 + 纹理坐标 + 切线)
        VertexLayout PBRVertex();

    } // namespace layouts

} // namespace zk_pbr::gfx

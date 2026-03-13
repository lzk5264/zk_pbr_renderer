#include <zk_pbr/gfx/vertex_layout.h>

#include <algorithm>

namespace zk_pbr::gfx
{

    void VertexLayout::AddAttribute(GLuint location, GLint size, GLenum type,
                                    GLboolean normalized, GLsizei stride, size_t offset)
    {
        // 验证参数
        if (size < 1 || size > 4)
        {
            throw std::invalid_argument("VertexAttribute size must be between 1 and 4");
        }
        if (stride < 0)
        {
            throw std::invalid_argument("VertexAttribute stride cannot be negative");
        }

        attributes_.emplace_back(location, size, type, normalized, stride, offset);
    }

    void VertexLayout::AddFloat(GLuint location, GLint size, GLsizei stride, size_t offset)
    {
        AddAttribute(location, size, GL_FLOAT, GL_FALSE, stride, offset);
    }

    void VertexLayout::Apply() const
    {
        for (const auto &attr : attributes_)
        {
            glEnableVertexAttribArray(attr.location);
            glVertexAttribPointer(attr.location, attr.size, attr.type, attr.normalized,
                                  attr.stride, reinterpret_cast<void *>(attr.offset));
        }
    }

    bool VertexLayout::IsValid() const noexcept
    {
        if (attributes_.empty())
            return false;

        return std::all_of(attributes_.begin(), attributes_.end(),
                           [](const VertexAttribute &attr)
                           { return attr.IsValid(); });
    }

    size_t VertexLayout::GetStride() const noexcept
    {
        if (attributes_.empty())
            return 0;
        return static_cast<size_t>(attributes_[0].stride);
    }

    namespace layouts
    {

        VertexLayout Position3D()
        {
            VertexLayout layout;
            layout.AddFloat(0, 3, 3 * sizeof(float), 0);
            return layout;
        }

        VertexLayout Position3DColor3D()
        {
            VertexLayout layout;
            layout.AddFloat(0, 3, 6 * sizeof(float), 0);
            layout.AddFloat(1, 3, 6 * sizeof(float), 3 * sizeof(float));
            return layout;
        }

        VertexLayout Position3DTexCoord2D()
        {
            VertexLayout layout;
            layout.AddFloat(0, 3, 5 * sizeof(float), 0);
            layout.AddFloat(1, 2, 5 * sizeof(float), 3 * sizeof(float));
            return layout;
        }

        VertexLayout Position3DNormal3DTexCoord2D()
        {
            VertexLayout layout;
            layout.AddFloat(0, 3, 8 * sizeof(float), 0);                 // position
            layout.AddFloat(1, 3, 8 * sizeof(float), 3 * sizeof(float)); // normal
            layout.AddFloat(2, 2, 8 * sizeof(float), 6 * sizeof(float)); // texCoord
            return layout;
        }

        VertexLayout PBRVertex()
        {
            // 与 pbr_shading_vs.vert 的 attribute location 严格对应：
            //   loc 0: vec3 position  (offset  0)
            //   loc 1: vec3 normal    (offset 12)
            //   loc 2: vec2 uv0       (offset 24)
            //   loc 3: vec4 tangent   (offset 32, w = handedness ±1)
            // 共 12 floats = 48 bytes/vertex。
            // glTF 的 TEXCOORD_1 (uv1) 当前不采样，暂不纳入顶点布局。
            constexpr GLsizei kStride = 12 * sizeof(float);
            VertexLayout layout;
            layout.AddFloat(0, 3, kStride, 0);                  // position
            layout.AddFloat(1, 3, kStride, 3 * sizeof(float));  // normal
            layout.AddFloat(2, 2, kStride, 6 * sizeof(float));  // uv0
            layout.AddFloat(3, 4, kStride, 8 * sizeof(float));  // tangent (xyz + handedness w)
            return layout;
        }

    } // namespace layouts

} // namespace zk_pbr::gfx

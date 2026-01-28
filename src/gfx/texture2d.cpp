#include <zk_pbr/gfx/texture2d.h>
#include <zk_pbr/gfx/texture.h>

namespace zk_pbr::gfx
{

    Texture2D::Texture2D(int width, int height, const TextureSpec &spec)
        : width_(width), height_(height)
    {
        glGenTextures(1, &id_);
        if (id_ == 0)
        {
            throw TextureException("Failed to create texture");
        }

        glBindTexture(GL_TEXTURE_2D, id_);

        // 分配存储
        glTexImage2D(GL_TEXTURE_2D, 0, spec.internal_format,
                     width, height, 0, spec.format, spec.data_type, nullptr);

        // 设置参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(spec.wrap));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(spec.wrap));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(spec.min_filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(spec.mag_filter));

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Texture2D::~Texture2D()
    {
        if (id_ != 0)
        {
            glDeleteTextures(1, &id_);
        }
    }

    Texture2D::Texture2D(Texture2D &&other) noexcept
        : id_(other.id_), width_(other.width_), height_(other.height_)
    {
        other.id_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }

    Texture2D &Texture2D::operator=(Texture2D &&other) noexcept
    {
        if (this != &other)
        {
            if (id_ != 0)
            {
                glDeleteTextures(1, &id_);
            }

            id_ = other.id_;
            width_ = other.width_;
            height_ = other.height_;

            other.id_ = 0;
            other.width_ = 0;
            other.height_ = 0;
        }
        return *this;
    }

    void Texture2D::Bind(unsigned int slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, id_);
    }

} // namespace zk_pbr::gfx

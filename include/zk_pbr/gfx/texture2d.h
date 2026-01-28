#pragma once

#include <glad/glad.h>

#include <zk_pbr/gfx/texture_parameters.h>

namespace zk_pbr::gfx
{

    // 2D 纹理 (RAII)
    // 用于 HDR FBO 颜色附件
    class Texture2D
    {
    public:
        Texture2D() = default;

        // 创建空纹理 (用于 FBO 附件)
        Texture2D(int width, int height, const TextureSpec &spec);

        ~Texture2D();

        // 禁止拷贝，允许移动
        Texture2D(const Texture2D &) = delete;
        Texture2D &operator=(const Texture2D &) = delete;
        Texture2D(Texture2D &&other) noexcept;
        Texture2D &operator=(Texture2D &&other) noexcept;

        void Bind(unsigned int slot = 0) const;

        GLuint GetID() const { return id_; }
        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }
        bool IsValid() const { return id_ != 0; }

    private:
        GLuint id_ = 0;
        int width_ = 0;
        int height_ = 0;
    };

} // namespace zk_pbr::gfx

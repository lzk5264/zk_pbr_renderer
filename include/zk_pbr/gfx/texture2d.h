#pragma once

#include <string>

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

        static Texture2D ComputeDFG(
            int size = 512,
            int sample_count = 1024,
            bool is_multiscatter = true,
            const TextureSpec &spec = TexturePresets::DFGLUT());

        void Bind(unsigned int slot = 0) const;

        // DEBUG: 将纹理导出为 PPM 图片（临时调试用，后续删除）
        void SaveToPPM(const std::string &path) const;

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

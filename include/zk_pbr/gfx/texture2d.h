#pragma once

#include <string>

#include <zk_pbr/gfx/texture.h>
#include <zk_pbr/gfx/texture_parameters.h>

namespace zk_pbr::gfx
{

    // 2D 纹理类
    class Texture2D
    {
    public:
        // 默认构造（空纹理）
        Texture2D() = default;

        // 从内存数据创建纹理
        Texture2D(int width, int height, const void *data, const TextureSpecification &spec);

        // 创建空纹理（用于 FBO 附件）
        Texture2D(int width, int height, const TextureSpecification &spec);

        Texture2D(const Texture2D &) = delete;
        Texture2D &operator=(const Texture2D &) = delete;

        Texture2D(Texture2D &&) noexcept = default;
        Texture2D &operator=(Texture2D &&) noexcept = default;

        ~Texture2D() = default;

        // 从文件加载（静态工厂方法）
        [[nodiscard]] static Texture2D LoadFromFile(const std::string &path,
                                                    const TextureSpecification &spec = TextureSpecification{});

        // 绑定到纹理单元
        void Bind(uint32_t slot = 0) const noexcept;
        void Unbind() const noexcept;

        // 更新纹理数据（部分或全部）
        void SetData(const void *data, int width, int height, int x_offset = 0, int y_offset = 0);

        // 重新生成 mipmap
        void GenerateMipmaps() noexcept;

        // 参数设置
        void SetWrapMode(TextureWrap wrap_s, TextureWrap wrap_t);
        void SetFilterMode(TextureFilter min_filter, TextureFilter mag_filter);
        void SetBorderColor(float r, float g, float b, float a);
        void SetAnisotropy(float level);

        // Getter
        [[nodiscard]] GLuint GetID() const noexcept { return handle_.Get(); }
        [[nodiscard]] int GetWidth() const noexcept { return width_; }
        [[nodiscard]] int GetHeight() const noexcept { return height_; }
        [[nodiscard]] bool IsValid() const noexcept { return handle_.IsValid(); }

    private:
        TextureHandle handle_;
        TextureSpecification spec_;
        int width_ = 0;
        int height_ = 0;

        void ApplyParameters();
        void ValidateDimensions(int width, int height) const;
    };

} // namespace zk_pbr::gfx

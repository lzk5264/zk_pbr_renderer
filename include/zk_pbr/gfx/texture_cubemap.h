#pragma once

#include <array>
#include <string>

#include <zk_pbr/gfx/texture.h>
#include <zk_pbr/gfx/texture_parameters.h>

namespace zk_pbr::gfx
{

    // Cubemap 纹理类
    class TextureCubemap
    {
    public:
        // 默认构造（空纹理）
        TextureCubemap() = default;

        // 从 6 个面的数据创建 cubemap
        // 顺序：+X, -X, +Y, -Y, +Z, -Z
        TextureCubemap(int width, int height,
                       const std::array<const void *, 6> &face_data,
                       const TextureSpecification &spec);

        // 创建空 cubemap（用于 FBO 附件或动态生成）
        TextureCubemap(int size, const TextureSpecification &spec);

        TextureCubemap(const TextureCubemap &) = delete;
        TextureCubemap &operator=(const TextureCubemap &) = delete;

        TextureCubemap(TextureCubemap &&) noexcept = default;
        TextureCubemap &operator=(TextureCubemap &&) noexcept = default;

        ~TextureCubemap() = default;

        // 从文件加载（6 张图片）
        // 路径顺序：+X(right), -X(left), +Y(top), -Y(bottom), +Z(front), -Z(back)
        [[nodiscard]] static TextureCubemap LoadFromFiles(
            const std::array<std::string, 6> &paths,
            const TextureSpecification &spec = TextureSpecification{});

        // 从单张等距柱状投影图加载（用于 HDR 环境贴图）
        [[nodiscard]] static TextureCubemap LoadFromEquirectangular(
            const std::string &path,
            int cubemap_size = 512,
            const TextureSpecification &spec = texture_presets::HDR());

        // 绑定到纹理单元
        void Bind(uint32_t slot = 0) const noexcept;
        void Unbind() const noexcept;

        // 更新单个面的数据
        void SetFaceData(uint32_t face_index, const void *data, int width, int height);

        // 重新生成 mipmap
        void GenerateMipmaps() noexcept;

        // 参数设置
        void SetWrapMode(TextureWrap wrap_s, TextureWrap wrap_t, TextureWrap wrap_r);
        void SetFilterMode(TextureFilter min_filter, TextureFilter mag_filter);

        // Getter
        [[nodiscard]] GLuint GetID() const noexcept { return handle_.Get(); }
        [[nodiscard]] int GetSize() const noexcept { return size_; }
        [[nodiscard]] bool IsValid() const noexcept { return handle_.IsValid(); }

        // Cubemap 面索引常量
        static constexpr uint32_t FACE_POSITIVE_X = 0; // Right
        static constexpr uint32_t FACE_NEGATIVE_X = 1; // Left
        static constexpr uint32_t FACE_POSITIVE_Y = 2; // Top
        static constexpr uint32_t FACE_NEGATIVE_Y = 3; // Bottom
        static constexpr uint32_t FACE_POSITIVE_Z = 4; // Front
        static constexpr uint32_t FACE_NEGATIVE_Z = 5; // Back

    private:
        TextureHandle handle_;
        TextureSpecification spec_;
        int size_ = 0;

        void ApplyParameters();
        void ValidateSize(int size) const;
        static GLenum GetCubemapFaceTarget(uint32_t face_index) noexcept;
    };

} // namespace zk_pbr::gfx

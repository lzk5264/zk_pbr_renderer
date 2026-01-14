#pragma once

#include <array>
#include <string>

#include <zk_pbr/gfx/texture.h>
#include <zk_pbr/gfx/texture_parameters.h>
#include <zk_pbr/gfx/texture2d.h>

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

        // 从等距矩形 HDR 贴图转换为 Cubemap（推荐用于 IBL 和环境贴图）
        // @param path: 等距矩形 HDR 文件路径（.hdr 或 .exr）
        // @param cubemap_size: 生成的 cubemap 每面尺寸
        // @param spec: Cubemap 纹理规格
        [[nodiscard]] static TextureCubemap LoadFromEquirectangular(
            const std::string &path,
            int cubemap_size = 512,
            const TextureSpecification &spec = texture_presets::HDRCubemap());

        // 从环境 Cubemap 生成 Irradiance Map（漫反射环境光照）
        // 使用蒙特卡洛采样进行半球余弦权重积分
        // @param source_cubemap: 源环境 cubemap
        // @param irradiance_size: 生成的 irradiance map 每面尺寸（通常 32-64 就够了）
        // @param sample_count: 每像素采样数（越多越准确，建议 256-1024）
        // @param spec: 输出 Cubemap 纹理规格
        [[nodiscard]] static TextureCubemap ConvolveIrradiance(
            const TextureCubemap &source_cubemap,
            int irradiance_size = 32,
            int sample_count = 512,
            const TextureSpecification &spec = texture_presets::HDRCubemap());

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
        [[nodiscard]] GLuint GetID() const noexcept
        {
            return handle_.Get();
        }
        [[nodiscard]] int GetSize() const noexcept
        {
            return size_;
        }
        [[nodiscard]] bool IsValid() const noexcept
        {
            return handle_.IsValid();
        }

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

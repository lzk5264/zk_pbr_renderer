#pragma once

#include <string>

#include <glad/glad.h>

#include <zk_pbr/gfx/texture_parameters.h>

namespace zk_pbr::gfx
{

    // Cubemap 纹理 (RAII)
    // 用于 IBL 环境贴图和 Irradiance Map
    class TextureCubemap
    {
    public:
        TextureCubemap() = default;

        // 创建空 Cubemap (用于 GPU 生成)
        TextureCubemap(int size, const TextureSpec &spec);

        ~TextureCubemap();

        // 禁止拷贝，允许移动
        TextureCubemap(const TextureCubemap &) = delete;
        TextureCubemap &operator=(const TextureCubemap &) = delete;
        TextureCubemap(TextureCubemap &&other) noexcept;
        TextureCubemap &operator=(TextureCubemap &&other) noexcept;

        // ===== IBL 核心功能 =====

        // 从等距矩形 HDR 贴图转换为 Cubemap
        static TextureCubemap LoadFromEquirectangular(
            const std::string &path,
            int cubemap_size = 512,
            const TextureSpec &spec = TexturePresets::HDRCubemap());

        // 从环境 Cubemap 生成 Irradiance Map
        static TextureCubemap ConvolveIrradiance(
            const TextureCubemap &source,
            int irradiance_size = 32,
            int sample_count = 512,
            const TextureSpec &spec = TexturePresets::IrradianceMap());

        void Bind(unsigned int slot = 0) const;
        void GenerateMipmaps();

        GLuint GetID() const { return id_; }
        int GetSize() const { return size_; }
        bool IsValid() const { return id_ != 0; }

    private:
        GLuint id_ = 0;
        int size_ = 0;
        TextureSpec spec_;

        void ApplyParameters();
    };

} // namespace zk_pbr::gfx

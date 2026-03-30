#pragma once

#include <string>

#include <glad/glad.h>

#include <zk_pbr/gfx/texture_parameters.h>

namespace zk_pbr::gfx
{

    /// OpenGL Cubemap 纹理的 RAII 封装，用于 IBL 环境光照管线。
    ///
    /// 该类管理 GL_TEXTURE_CUBE_MAP 资源的生命周期，提供三个核心静态工厂方法：
    ///   1. LoadFromEquirectangular - 从等距矩形 HDR 贴图转换为 Cubemap
    ///   2. ConvolveIrradiance      - 生成漫反射项的 Irradiance Map（低频卷积）
    ///   3. PrefilteredEnvMap       - 生成镜面反射项的 Prefiltered Env Map（多粗糙度 mipmap）
    ///
    /// 典型使用场景（SceneEnvironment 初始化）：
    ///   auto env = TextureCubemap::LoadFromEquirectangular("env.hdr", 1024);
    ///   auto irr = TextureCubemap::ConvolveIrradiance(env, 32, 512);
    ///   auto pref = TextureCubemap::PrefilteredEnvMap(env, 256, 1024);
    /// TODO: 动态天空盒渲染
    class TextureCubemap
    {
    public:
        TextureCubemap() = default;

        /// 创建空 Cubemap（预分配显存，用于后续 FBO 渲染）。
        ///
        /// @param size 每个面的分辨率（正方形，单位：像素）
        /// @param spec 纹理参数（internal format、filter、wrap、是否生成 mipmap）
        TextureCubemap(int size, const TextureSpec &spec);

        ~TextureCubemap();

        TextureCubemap(const TextureCubemap &) = delete;
        TextureCubemap &operator=(const TextureCubemap &) = delete;
        TextureCubemap(TextureCubemap &&other) noexcept;
        TextureCubemap &operator=(TextureCubemap &&other) noexcept;

        // ===== IBL 核心功能 =====

        /// 从等距矩形 HDR 贴图转换为 Cubemap
        ///
        /// @param path          等距矩形贴图路径（支持 .exr/.hdr）
        /// @param cubemap_size  输出 Cubemap 分辨率（每面），推荐 512-2048
        /// @param spec          纹理参数，默认 RGB16F + mipmap
        /// @return              转换后的 Cubemap，失败时抛出 TextureException
        static TextureCubemap LoadFromEquirectangular(
            const std::string &path,
            int cubemap_size = 512,
            const TextureSpec &spec = TexturePresets::HDRCubemap());

        /// 从环境 Cubemap 生成 Irradiance Map
        ///
        /// @param source           源环境 Cubemap
        /// @param irradiance_size  输出分辨率，低频信号 32 * 32 足够
        /// @param sample_count     蒙特卡洛采样数，512 可平衡质量与速度
        /// @param spec             纹理参数，默认 RGB16F 无 mipmap
        /// @return                 Irradiance Map，失败时抛出 TextureException
        static TextureCubemap ConvolveIrradiance(
            const TextureCubemap &source,
            int irradiance_size = 32,
            int sample_count = 512,
            const TextureSpec &spec = TexturePresets::IrradianceMap());

        /// 从环境 Cubemap 生成 Prefiltered Env Map
        ///
        /// @param source         源环境 Cubemap
        /// @param size           输出分辨率（mip 0），推荐 256（512 收益递减）
        /// @param sample_count   每像素采样数
        /// @param spec           纹理参数，默认 RGB16F + mipmap
        /// @return               Prefiltered Env Map，失败时抛出 TextureException
        static TextureCubemap PrefilteredEnvMap(
            const TextureCubemap &source,
            int size = 256,
            int sample_count = 1024,
            const TextureSpec &spec = TexturePresets::PrefilteredEnvMap());

        /// 将 Cubemap 绑定到指定纹理单元
        ///
        /// @param slot 纹理单元索引（对应 shader 中的 binding = N）
        void Bind(unsigned int slot = 0) const;

        /// 生成 mipmap chain（调用 glGenerateMipmap）。
        ///
        /// 注意：仅当 spec.generate_mipmaps = true 时才需要调用。
        ///       LoadFromEquirectangular 会自动调用此方法。
        void GenerateMipmaps();

        GLuint GetID() const { return id_; }
        int GetSize() const { return size_; }
        bool IsValid() const { return id_ != 0; }

    private:
        GLuint id_ = 0;
        int size_ = 0;
        TextureSpec spec_;

        /// 应用纹理参数（wrap、filter）到当前绑定的 Cubemap。
        ///
        /// 前置条件：glBindTexture(GL_TEXTURE_CUBE_MAP, id_) 已调用。
        void ApplyParameters();
    };

} // namespace zk_pbr::gfx

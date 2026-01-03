#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <glad/glad.h>

#include <zk_pbr/gfx/texture_parameters.h>

namespace zk_pbr::gfx
{

    // 纹理异常类
    struct TextureException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // 纹理句柄封装（RAII，内部使用）
    class TextureHandle
    {
    public:
        TextureHandle() = default;
        explicit TextureHandle(GLenum target);

        TextureHandle(const TextureHandle &) = delete;
        TextureHandle &operator=(const TextureHandle &) = delete;

        TextureHandle(TextureHandle &&other) noexcept;
        TextureHandle &operator=(TextureHandle &&other) noexcept;

        ~TextureHandle() noexcept;

        [[nodiscard]] GLuint Get() const noexcept { return id_; }
        [[nodiscard]] GLenum GetTarget() const noexcept { return target_; }
        [[nodiscard]] bool IsValid() const noexcept { return id_ != 0; }

        void Bind() const noexcept;
        void Unbind() const noexcept;

    private:
        GLuint id_ = 0;
        GLenum target_ = 0;

        void Cleanup() noexcept;
    };

    // 纹理加载结果
    struct TextureData
    {
        std::unique_ptr<uint8_t[], void (*)(void *)> pixels;
        int width = 0;
        int height = 0;
        int channels = 0;
        bool is_hdr = false;

        TextureData() : pixels(nullptr, [](void *) {}) {}

        [[nodiscard]] bool IsValid() const noexcept
        {
            return pixels != nullptr && width > 0 && height > 0 && channels > 0;
        }
    };

    // 纹理加载器（使用 stb_image）
    class TextureLoader
    {
    public:
        // 加载 LDR 图像
        [[nodiscard]] static TextureData LoadFromFile(const std::string &path, bool flip_vertically = true);

        // 加载 HDR 图像
        [[nodiscard]] static TextureData LoadHDRFromFile(const std::string &path, bool flip_vertically = true);

        // 设置期望的通道数（0 = 保持原始）
        static void SetDesiredChannels(int channels) noexcept { desired_channels_ = channels; }

    private:
        static int desired_channels_;
    };

} // namespace zk_pbr::gfx

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// tinyexr（单头文件方式，使用 release 版本）
#define TINYEXR_USE_MINIZ 1
#define TINYEXR_USE_THREAD 0
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

#include <zk_pbr/gfx/texture.h>

namespace zk_pbr::gfx
{

    // ========== TextureHandle 实现 ==========

    TextureHandle::TextureHandle(GLenum target) : target_(target)
    {
        glGenTextures(1, &id_);
        if (id_ == 0)
        {
            throw TextureException("Failed to generate texture object");
        }
    }

    TextureHandle::TextureHandle(TextureHandle &&other) noexcept
        : id_(std::exchange(other.id_, 0)),
          target_(std::exchange(other.target_, 0))
    {
    }

    TextureHandle &TextureHandle::operator=(TextureHandle &&other) noexcept
    {
        if (this != &other)
        {
            Cleanup();
            id_ = std::exchange(other.id_, 0);
            target_ = std::exchange(other.target_, 0);
        }
        return *this;
    }

    TextureHandle::~TextureHandle() noexcept
    {
        Cleanup();
    }

    void TextureHandle::Bind() const noexcept
    {
        if (id_ != 0)
        {
            glBindTexture(target_, id_);
        }
    }

    void TextureHandle::Unbind() const noexcept
    {
        if (target_ != 0)
        {
            glBindTexture(target_, 0);
        }
    }

    void TextureHandle::Cleanup() noexcept
    {
        if (id_ != 0)
        {
            glDeleteTextures(1, &id_);
            id_ = 0;
        }
    }

    // ========== TextureLoader 实现 ==========

    int TextureLoader::desired_channels_ = 0;

    TextureData TextureLoader::LoadFromFile(const std::string &path, bool flip_vertically)
    {
        TextureData data;

        stbi_set_flip_vertically_on_load(flip_vertically);

        int width, height, channels;
        unsigned char *pixels = stbi_load(path.c_str(), &width, &height, &channels, desired_channels_);

        if (!pixels)
        {
            throw TextureException("Failed to load texture: " + path + "\nReason: " + stbi_failure_reason());
        }

        data.pixels = std::unique_ptr<uint8_t[], void (*)(void *)>(
            pixels, [](void *ptr)
            { stbi_image_free(ptr); });
        data.width = width;
        data.height = height;
        data.channels = (desired_channels_ != 0) ? desired_channels_ : channels;
        data.is_hdr = false;

        return data;
    }

    TextureData TextureLoader::LoadHDRFromFile(const std::string &path, bool flip_vertically)
    {
        TextureData data;

        // 检查是否是 EXR 格式
        bool is_exr = (path.find(".exr") != std::string::npos || path.find(".EXR") != std::string::npos);

        if (is_exr)
        {
            // 使用 tinyexr 加载 EXR
            float *pixels = nullptr;
            int width, height;
            const char *err = nullptr;

            int ret = LoadEXR(&pixels, &width, &height, path.c_str(), &err);

            if (ret != TINYEXR_SUCCESS)
            {
                std::string error_msg = err ? err : "Unknown error";
                if (err)
                    FreeEXRErrorMessage(err);
                throw TextureException("Failed to load EXR texture: " + path + "\nReason: " + error_msg);
            }

            // EXR 加载的数据是 RGBA 格式（4 通道）
            int channels = 4;

            // 如果需要翻转
            if (flip_vertically)
            {
                // 手动翻转图像
                int row_size = width * channels;
                std::vector<float> temp_row(row_size);
                for (int y = 0; y < height / 2; ++y)
                {
                    float *row1 = pixels + y * row_size;
                    float *row2 = pixels + (height - 1 - y) * row_size;
                    std::memcpy(temp_row.data(), row1, row_size * sizeof(float));
                    std::memcpy(row1, row2, row_size * sizeof(float));
                    std::memcpy(row2, temp_row.data(), row_size * sizeof(float));
                }
            }

            data.pixels = std::unique_ptr<uint8_t[], void (*)(void *)>(
                reinterpret_cast<uint8_t *>(pixels), [](void *ptr)
                { free(ptr); });
            data.width = width;
            data.height = height;
            data.channels = channels;
            data.is_hdr = true;
        }
        else
        {
            // 使用 stb_image 加载 HDR
            stbi_set_flip_vertically_on_load(flip_vertically);

            int width, height, channels;
            float *pixels = stbi_loadf(path.c_str(), &width, &height, &channels, desired_channels_);

            if (!pixels)
            {
                throw TextureException("Failed to load HDR texture: " + path + "\nReason: " + stbi_failure_reason());
            }

            data.pixels = std::unique_ptr<uint8_t[], void (*)(void *)>(
                reinterpret_cast<uint8_t *>(pixels), [](void *ptr)
                { stbi_image_free(ptr); });
            data.width = width;
            data.height = height;
            data.channels = (desired_channels_ != 0) ? desired_channels_ : channels;
            data.is_hdr = true;
        }

        return data;
    }

} // namespace zk_pbr::gfx

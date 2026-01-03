#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

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

        stbi_set_flip_vertically_on_load(flip_vertically);

        int width, height, channels;
        float *pixels = stbi_loadf(path.c_str(), &width, &height, &channels, desired_channels_);

        if (!pixels)
        {
            throw TextureException("Failed to load HDR texture: " + path + "\nReason: " + stbi_failure_reason());
        }

        // 将 float* 转换为 uint8_t* 存储（实际上存储的是 float 数据）
        data.pixels = std::unique_ptr<uint8_t[], void (*)(void *)>(
            reinterpret_cast<uint8_t *>(pixels), [](void *ptr)
            { stbi_image_free(ptr); });
        data.width = width;
        data.height = height;
        data.channels = (desired_channels_ != 0) ? desired_channels_ : channels;
        data.is_hdr = true;

        return data;
    }

} // namespace zk_pbr::gfx

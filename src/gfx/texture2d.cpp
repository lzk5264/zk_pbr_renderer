#include <zk_pbr/gfx/texture2d.h>

#include <algorithm>

// 各向异性过滤扩展常量（如果系统未定义）
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

namespace zk_pbr::gfx
{

    // ========== Texture2D 实现 ==========

    Texture2D::Texture2D(int width, int height, const void *data, const TextureSpecification &spec)
        : handle_(GL_TEXTURE_2D), spec_(spec), width_(width), height_(height)
    {
        ValidateDimensions(width, height);

        handle_.Bind();

        // 确定格式
        GLenum internal_format = static_cast<GLenum>(spec_.internal_format);
        GLenum format = static_cast<GLenum>(spec_.format);
        GLenum type = static_cast<GLenum>(spec_.data_type);

        // 上传纹理数据
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, data);

        // 应用参数
        ApplyParameters();

        // 生成 mipmap
        if (spec_.generate_mipmaps && data != nullptr)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        handle_.Unbind();
    }

    Texture2D::Texture2D(int width, int height, const TextureSpecification &spec)
        : Texture2D(width, height, nullptr, spec)
    {
    }

    Texture2D Texture2D::LoadFromFile(const std::string &path, const TextureSpecification &spec)
    {
        // 加载图像数据
        bool is_hdr = (path.find(".hdr") != std::string::npos || path.find(".exr") != std::string::npos);

        TextureData texture_data = is_hdr ? TextureLoader::LoadHDRFromFile(path, spec.flip_vertically)
                                          : TextureLoader::LoadFromFile(path, spec.flip_vertically);

        if (!texture_data.IsValid())
        {
            throw TextureException("Failed to load texture from file: " + path);
        }

        // 根据通道数确定格式
        TextureSpecification final_spec = spec;

        if (!is_hdr)
        {
            // LDR 纹理
            switch (texture_data.channels)
            {
            case 1:
                final_spec.format = TextureFormat::Red;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                {
                    final_spec.internal_format = TextureInternalFormat::R8;
                }
                break;
            case 2:
                final_spec.format = TextureFormat::RG;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                {
                    final_spec.internal_format = TextureInternalFormat::RG8;
                }
                break;
            case 3:
                final_spec.format = TextureFormat::RGB;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                {
                    final_spec.internal_format = spec.srgb ? TextureInternalFormat::SRGB8 : TextureInternalFormat::RGB8;
                }
                break;
            case 4:
                final_spec.format = TextureFormat::RGBA;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                {
                    final_spec.internal_format = spec.srgb ? TextureInternalFormat::SRGB8_ALPHA8 : TextureInternalFormat::RGBA8;
                }
                break;
            default:
                throw TextureException("Unsupported channel count: " + std::to_string(texture_data.channels));
            }
        }
        else
        {
            // HDR 纹理
            final_spec.data_type = TextureDataType::Float;
            switch (texture_data.channels)
            {
            case 1:
                final_spec.format = TextureFormat::Red;
                final_spec.internal_format = TextureInternalFormat::R32F;
                break;
            case 2:
                final_spec.format = TextureFormat::RG;
                final_spec.internal_format = TextureInternalFormat::RG32F;
                break;
            case 3:
                final_spec.format = TextureFormat::RGB;
                final_spec.internal_format = TextureInternalFormat::RGB32F;
                break;
            case 4:
                final_spec.format = TextureFormat::RGBA;
                final_spec.internal_format = TextureInternalFormat::RGBA32F;
                break;
            default:
                throw TextureException("Unsupported HDR channel count: " + std::to_string(texture_data.channels));
            }
        }

        return Texture2D(texture_data.width, texture_data.height, texture_data.pixels.get(), final_spec);
    }

    void Texture2D::Bind(uint32_t slot) const noexcept
    {
        if (handle_.IsValid())
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            handle_.Bind();
        }
    }

    void Texture2D::Unbind() const noexcept
    {
        handle_.Unbind();
    }

    void Texture2D::SetData(const void *data, int width, int height, int x_offset, int y_offset)
    {
        if (!data)
        {
            throw TextureException("SetData: data pointer is null");
        }
        if (x_offset + width > width_ || y_offset + height > height_)
        {
            throw TextureException("SetData: region exceeds texture bounds");
        }

        handle_.Bind();
        glTexSubImage2D(GL_TEXTURE_2D, 0, x_offset, y_offset, width, height,
                        static_cast<GLenum>(spec_.format),
                        static_cast<GLenum>(spec_.data_type),
                        data);
        handle_.Unbind();
    }

    void Texture2D::GenerateMipmaps() noexcept
    {
        if (handle_.IsValid())
        {
            handle_.Bind();
            glGenerateMipmap(GL_TEXTURE_2D);
            handle_.Unbind();
        }
    }

    void Texture2D::SetWrapMode(TextureWrap wrap_s, TextureWrap wrap_t)
    {
        spec_.wrap_s = wrap_s;
        spec_.wrap_t = wrap_t;

        handle_.Bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap_s));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap_t));
        handle_.Unbind();
    }

    void Texture2D::SetFilterMode(TextureFilter min_filter, TextureFilter mag_filter)
    {
        spec_.min_filter = min_filter;
        spec_.mag_filter = mag_filter;

        handle_.Bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(min_filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(mag_filter));
        handle_.Unbind();
    }

    void Texture2D::SetBorderColor(float r, float g, float b, float a)
    {
        spec_.border_color[0] = r;
        spec_.border_color[1] = g;
        spec_.border_color[2] = b;
        spec_.border_color[3] = a;

        handle_.Bind();
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, spec_.border_color);
        handle_.Unbind();
    }

    void Texture2D::SetAnisotropy(float level)
    {
        if (level <= 0.0f)
            return;

        // 获取最大支持的各向异性级别
        float max_anisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);

        spec_.anisotropy = std::min(level, max_anisotropy);

        handle_.Bind();
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, spec_.anisotropy);
        handle_.Unbind();
    }

    void Texture2D::ApplyParameters()
    {
        // 环绕模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(spec_.wrap_s));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(spec_.wrap_t));

        // 过滤模式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(spec_.min_filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(spec_.mag_filter));

        // 边界颜色
        if (spec_.wrap_s == TextureWrap::ClampToBorder || spec_.wrap_t == TextureWrap::ClampToBorder)
        {
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, spec_.border_color);
        }

        // 各向异性过滤
        if (spec_.anisotropy > 0.0f)
        {
            float max_anisotropy = 0.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
            float final_anisotropy = std::min(spec_.anisotropy, max_anisotropy);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, final_anisotropy);
        }
    }

    void Texture2D::ValidateDimensions(int width, int height) const
    {
        if (width <= 0 || height <= 0)
        {
            throw TextureException("Invalid texture dimensions: " + std::to_string(width) + "x" + std::to_string(height));
        }

        // 检查是否超过 OpenGL 限制
        GLint max_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

        if (width > max_size || height > max_size)
        {
            throw TextureException("Texture dimensions exceed maximum size: " + std::to_string(max_size));
        }
    }

} // namespace zk_pbr::gfx

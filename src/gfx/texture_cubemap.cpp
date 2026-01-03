#include <zk_pbr/gfx/texture_cubemap.h>

#include <algorithm>

namespace zk_pbr::gfx
{

    // ========== TextureCubemap 实现 ==========

    TextureCubemap::TextureCubemap(int width, int height,
                                   const std::array<const void *, 6> &face_data,
                                   const TextureSpecification &spec)
        : handle_(GL_TEXTURE_CUBE_MAP), spec_(spec), size_(width)
    {
        ValidateSize(width);

        if (width != height)
        {
            throw TextureException("Cubemap faces must be square");
        }

        handle_.Bind();

        GLenum internal_format = static_cast<GLenum>(spec_.internal_format);
        GLenum format = static_cast<GLenum>(spec_.format);
        GLenum type = static_cast<GLenum>(spec_.data_type);

        // 上传 6 个面的数据
        for (uint32_t i = 0; i < 6; ++i)
        {
            GLenum target = GetCubemapFaceTarget(i);
            glTexImage2D(target, 0, internal_format, width, height, 0, format, type, face_data[i]);
        }

        // 应用参数
        ApplyParameters();

        // 生成 mipmap
        if (spec_.generate_mipmaps)
        {
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        }

        handle_.Unbind();
    }

    TextureCubemap::TextureCubemap(int size, const TextureSpecification &spec)
        : handle_(GL_TEXTURE_CUBE_MAP), spec_(spec), size_(size)
    {
        ValidateSize(size);

        handle_.Bind();

        GLenum internal_format = static_cast<GLenum>(spec_.internal_format);
        GLenum format = static_cast<GLenum>(spec_.format);
        GLenum type = static_cast<GLenum>(spec_.data_type);

        // 创建空的 cubemap（6 个面）
        for (uint32_t i = 0; i < 6; ++i)
        {
            GLenum target = GetCubemapFaceTarget(i);
            glTexImage2D(target, 0, internal_format, size, size, 0, format, type, nullptr);
        }

        // 应用参数
        ApplyParameters();

        handle_.Unbind();
    }

    TextureCubemap TextureCubemap::LoadFromFiles(
        const std::array<std::string, 6> &paths,
        const TextureSpecification &spec)
    {
        std::array<TextureData, 6> face_data;
        int size = 0;

        // 加载所有面
        for (uint32_t i = 0; i < 6; ++i)
        {
            bool is_hdr = (paths[i].find(".hdr") != std::string::npos ||
                           paths[i].find(".exr") != std::string::npos);

            face_data[i] = is_hdr ? TextureLoader::LoadHDRFromFile(paths[i], false)
                                  : TextureLoader::LoadFromFile(paths[i], false); // cubemap 通常不翻转

            if (!face_data[i].IsValid())
            {
                throw TextureException("Failed to load cubemap face: " + paths[i]);
            }

            // 验证尺寸
            if (face_data[i].width != face_data[i].height)
            {
                throw TextureException("Cubemap face must be square: " + paths[i]);
            }

            if (i == 0)
            {
                size = face_data[i].width;
            }
            else if (face_data[i].width != size || face_data[i].height != size)
            {
                throw TextureException("All cubemap faces must have the same size. Face " +
                                       std::to_string(i) + " size mismatch: " + paths[i]);
            }
        }

        // 确定格式
        TextureSpecification final_spec = spec;
        int channels = face_data[0].channels;
        bool is_hdr = face_data[0].is_hdr;

        if (!is_hdr)
        {
            switch (channels)
            {
            case 1:
                final_spec.format = TextureFormat::Red;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::R8;
                break;
            case 2:
                final_spec.format = TextureFormat::RG;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::RG8;
                break;
            case 3:
                final_spec.format = TextureFormat::RGB;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::RGB8;
                break;
            case 4:
                final_spec.format = TextureFormat::RGBA;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::RGBA8;
                break;
            }
        }
        else
        {
            final_spec.data_type = TextureDataType::Float;
            switch (channels)
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
            }
        }

        // 准备数据指针数组
        std::array<const void *, 6> data_ptrs;
        for (int i = 0; i < 6; ++i)
        {
            data_ptrs[i] = face_data[i].pixels.get();
        }

        return TextureCubemap(size, size, data_ptrs, final_spec);
    }

    TextureCubemap TextureCubemap::LoadFromEquirectangular(
        const std::string &path,
        int cubemap_size,
        const TextureSpecification &spec)
    {
        // TODO: 实现等距柱状投影转 cubemap
        // 这需要一个着色器来做投影转换
        // 暂时抛出异常提示未实现
        throw TextureException("LoadFromEquirectangular is not yet implemented. "
                               "This requires a shader-based conversion.");
    }

    void TextureCubemap::Bind(uint32_t slot) const noexcept
    {
        if (handle_.IsValid())
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            handle_.Bind();
        }
    }

    void TextureCubemap::Unbind() const noexcept
    {
        handle_.Unbind();
    }

    void TextureCubemap::SetFaceData(uint32_t face_index, const void *data, int width, int height)
    {
        if (face_index >= 6)
        {
            throw TextureException("SetFaceData: invalid face index " + std::to_string(face_index));
        }
        if (!data)
        {
            throw TextureException("SetFaceData: data pointer is null");
        }
        if (width != size_ || height != size_)
        {
            throw TextureException("SetFaceData: face size mismatch");
        }

        handle_.Bind();
        GLenum target = GetCubemapFaceTarget(face_index);
        glTexSubImage2D(target, 0, 0, 0, width, height,
                        static_cast<GLenum>(spec_.format),
                        static_cast<GLenum>(spec_.data_type),
                        data);
        handle_.Unbind();
    }

    void TextureCubemap::GenerateMipmaps() noexcept
    {
        if (handle_.IsValid())
        {
            handle_.Bind();
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            handle_.Unbind();
        }
    }

    void TextureCubemap::SetWrapMode(TextureWrap wrap_s, TextureWrap wrap_t, TextureWrap wrap_r)
    {
        spec_.wrap_s = wrap_s;
        spec_.wrap_t = wrap_t;
        spec_.wrap_r = wrap_r;

        handle_.Bind();
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap_s));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap_t));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, static_cast<GLint>(wrap_r));
        handle_.Unbind();
    }

    void TextureCubemap::SetFilterMode(TextureFilter min_filter, TextureFilter mag_filter)
    {
        spec_.min_filter = min_filter;
        spec_.mag_filter = mag_filter;

        handle_.Bind();
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(min_filter));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(mag_filter));
        handle_.Unbind();
    }

    void TextureCubemap::ApplyParameters()
    {
        // 环绕模式
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, static_cast<GLint>(spec_.wrap_s));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, static_cast<GLint>(spec_.wrap_t));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, static_cast<GLint>(spec_.wrap_r));

        // 过滤模式
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(spec_.min_filter));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(spec_.mag_filter));

        // Cubemap 默认使用 ClampToEdge 防止接缝
        if (spec_.wrap_s == TextureWrap::Repeat || spec_.wrap_t == TextureWrap::Repeat || spec_.wrap_r == TextureWrap::Repeat)
        {
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
    }

    void TextureCubemap::ValidateSize(int size) const
    {
        if (size <= 0)
        {
            throw TextureException("Invalid cubemap size: " + std::to_string(size));
        }

        // 检查是否超过 OpenGL 限制
        GLint max_size = 0;
        glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max_size);

        if (size > max_size)
        {
            throw TextureException("Cubemap size exceeds maximum: " + std::to_string(max_size));
        }
    }

    GLenum TextureCubemap::GetCubemapFaceTarget(uint32_t face_index) noexcept
    {
        static const GLenum targets[6] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_X, // Right
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X, // Left
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y, // Top
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, // Bottom
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z, // Front
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z  // Back
        };

        return (face_index < 6) ? targets[face_index] : GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

} // namespace zk_pbr::gfx

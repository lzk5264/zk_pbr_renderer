#pragma once

#include <glad/glad.h>
#include <stdexcept>
#include <string>

namespace zk_pbr::gfx
{

    // ============================================================================
    // 纹理异常类
    // ============================================================================

    class TextureException : public std::runtime_error
    {
    public:
        explicit TextureException(const std::string &message)
            : std::runtime_error(message)
        {
        }
    };

    // ============================================================================
    // 纹理参数枚举 (类型安全的 OpenGL 常量封装)
    // ============================================================================

    enum class TextureWrap : GLenum
    {
        Repeat = GL_REPEAT,
        ClampToEdge = GL_CLAMP_TO_EDGE,
        ClampToBorder = GL_CLAMP_TO_BORDER
    };

    enum class TextureFilter : GLenum
    {
        Nearest = GL_NEAREST,
        Linear = GL_LINEAR,
        LinearMipmapLinear = GL_LINEAR_MIPMAP_LINEAR
    };

    // ============================================================================
    // 纹理规格
    // ============================================================================

    struct TextureSpec
    {
        GLint internal_format = GL_RGBA8;
        GLenum format = GL_RGBA;
        GLenum data_type = GL_UNSIGNED_BYTE;

        TextureWrap wrap = TextureWrap::Repeat;
        TextureFilter min_filter = TextureFilter::LinearMipmapLinear;
        TextureFilter mag_filter = TextureFilter::Linear;

        bool generate_mipmaps = true;
    };

    // ============================================================================
    // 常用预设
    // ============================================================================

    namespace TexturePresets
    {
        // HDR Framebuffer 颜色附件
        inline TextureSpec HDRRenderTarget()
        {
            TextureSpec spec;
            spec.internal_format = GL_RGBA16F;
            spec.format = GL_RGBA;
            spec.data_type = GL_FLOAT;
            spec.wrap = TextureWrap::ClampToEdge;
            spec.min_filter = TextureFilter::Linear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = false;
            return spec;
        }

        // HDR Cubemap (用于环境贴图)
        inline TextureSpec HDRCubemap()
        {
            TextureSpec spec;
            spec.internal_format = GL_RGB16F;
            spec.format = GL_RGB;
            spec.data_type = GL_FLOAT;
            spec.wrap = TextureWrap::ClampToEdge;
            spec.min_filter = TextureFilter::LinearMipmapLinear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = true;
            return spec;
        }

        // Irradiance Map (不需要 mipmap)
        inline TextureSpec IrradianceMap()
        {
            TextureSpec spec;
            spec.internal_format = GL_RGB16F;
            spec.format = GL_RGB;
            spec.data_type = GL_FLOAT;
            spec.wrap = TextureWrap::ClampToEdge;
            spec.min_filter = TextureFilter::Linear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = false;
            return spec;
        }

        // Prefiltered Env Map (需要 mipmap)
        inline TextureSpec PrefilteredEnvMap()
        {
            return HDRCubemap();
        }

        inline TextureSpec DFGLUT()
        {
            TextureSpec spec;
            spec.internal_format = GL_RG16F;
            spec.format = GL_RG;
            spec.data_type = GL_FLOAT;
            spec.wrap = TextureWrap::ClampToEdge;
            spec.min_filter = TextureFilter::Linear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = false;
            return spec;
        }

    } // namespace TexturePresets

} // namespace zk_pbr::gfx

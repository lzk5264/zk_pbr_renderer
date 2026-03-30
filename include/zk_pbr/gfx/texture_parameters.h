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
        using std::runtime_error::runtime_error;
    };

    // ============================================================================
    // 纹理参数枚举 (类型安全的 OpenGL 常量封装)
    // ============================================================================

    enum class TextureWrap : GLenum
    {
        Repeat = GL_REPEAT,
        ClampToEdge = GL_CLAMP_TO_EDGE,
        ClampToBorder = GL_CLAMP_TO_BORDER,
        Mirrored_repeat = GL_MIRRORED_REPEAT
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

        // 给 baseColor/emissive 等颜色类型的纹理使用的默认预设
        inline TextureSpec LDRColorMap()
        {
            TextureSpec spec;
            spec.internal_format = GL_SRGB8_ALPHA8; // 颜色纹理需要启用 sRGB
            spec.format = GL_RGBA;
            spec.data_type = GL_UNSIGNED_BYTE;
            spec.wrap = TextureWrap::Repeat;
            spec.min_filter = TextureFilter::LinearMipmapLinear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = true;
            return spec;
        }

        inline TextureSpec LDRDataMap()
        {
            TextureSpec spec;
            spec.internal_format = GL_RGBA8; // Data 不用 sRGB 格式
            spec.format = GL_RGBA;
            spec.data_type = GL_UNSIGNED_BYTE;
            spec.wrap = TextureWrap::Repeat;
            spec.min_filter = TextureFilter::LinearMipmapLinear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = true;
            return spec;
        }

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

        // Shadow Map 深度纹理
        inline TextureSpec ShadowMap()
        {
            TextureSpec spec;
            spec.internal_format = GL_DEPTH_COMPONENT24;
            spec.format = GL_DEPTH_COMPONENT;
            spec.data_type = GL_FLOAT;
            spec.wrap = TextureWrap::ClampToBorder;
            spec.min_filter = TextureFilter::Nearest;
            spec.mag_filter = TextureFilter::Nearest;
            spec.generate_mipmaps = false;
            return spec;
        }

        // Bloom mip chain (HDR, 不需要 mipmap)
        inline TextureSpec BloomMip()
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

    } // namespace TexturePresets

} // namespace zk_pbr::gfx

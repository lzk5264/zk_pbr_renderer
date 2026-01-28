#pragma once

#include <glad/glad.h>

namespace zk_pbr::gfx
{

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
    // 纹理规格 (简化版，只保留 IBL 需要的)
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
    // 常用预设 (只保留 IBL 需要的)
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

    } // namespace TexturePresets

} // namespace zk_pbr::gfx

#pragma once

#include <glad/glad.h>

namespace zk_pbr::gfx
{

    // 纹理环绕模式
    enum class TextureWrap : GLenum
    {
        Repeat = GL_REPEAT,
        MirroredRepeat = GL_MIRRORED_REPEAT,
        ClampToEdge = GL_CLAMP_TO_EDGE,
        ClampToBorder = GL_CLAMP_TO_BORDER
    };

    // 纹理过滤模式
    enum class TextureFilter : GLenum
    {
        Nearest = GL_NEAREST,
        Linear = GL_LINEAR,
        NearestMipmapNearest = GL_NEAREST_MIPMAP_NEAREST,
        LinearMipmapNearest = GL_LINEAR_MIPMAP_NEAREST,
        NearestMipmapLinear = GL_NEAREST_MIPMAP_LINEAR,
        LinearMipmapLinear = GL_LINEAR_MIPMAP_LINEAR
    };

    // 纹理内部格式
    enum class TextureInternalFormat : GLint
    {
        // 常规格式
        R8 = GL_R8,
        RG8 = GL_RG8,
        RGB8 = GL_RGB8,
        RGBA8 = GL_RGBA8,

        // HDR 格式
        R16F = GL_R16F,
        RG16F = GL_RG16F,
        RGB16F = GL_RGB16F,
        RGBA16F = GL_RGBA16F,

        R32F = GL_R32F,
        RG32F = GL_RG32F,
        RGB32F = GL_RGB32F,
        RGBA32F = GL_RGBA32F,

        // sRGB 格式
        SRGB8 = GL_SRGB8,
        SRGB8_ALPHA8 = GL_SRGB8_ALPHA8,

        // 深度/模板格式
        Depth24Stencil8 = GL_DEPTH24_STENCIL8,
        Depth32F = GL_DEPTH_COMPONENT32F,
        Depth16 = GL_DEPTH_COMPONENT16
    };

    // 纹理数据格式
    enum class TextureFormat : GLenum
    {
        Red = GL_RED,
        RG = GL_RG,
        RGB = GL_RGB,
        RGBA = GL_RGBA,
        DepthComponent = GL_DEPTH_COMPONENT,
        DepthStencil = GL_DEPTH_STENCIL
    };

    // 纹理数据类型
    enum class TextureDataType : GLenum
    {
        UnsignedByte = GL_UNSIGNED_BYTE,
        Byte = GL_BYTE,
        UnsignedShort = GL_UNSIGNED_SHORT,
        Short = GL_SHORT,
        UnsignedInt = GL_UNSIGNED_INT,
        Int = GL_INT,
        Float = GL_FLOAT,
        UnsignedInt24_8 = GL_UNSIGNED_INT_24_8
    };

    // 纹理参数配置
    struct TextureSpecification
    {
        TextureInternalFormat internal_format = TextureInternalFormat::RGBA8;
        TextureFormat format = TextureFormat::RGBA;
        TextureDataType data_type = TextureDataType::UnsignedByte;

        TextureWrap wrap_s = TextureWrap::Repeat;
        TextureWrap wrap_t = TextureWrap::Repeat;
        TextureWrap wrap_r = TextureWrap::Repeat;

        TextureFilter min_filter = TextureFilter::LinearMipmapLinear;
        TextureFilter mag_filter = TextureFilter::Linear;

        bool generate_mipmaps = true;
        bool flip_vertically = true; // stb_image 默认需要翻转
        bool srgb = false;           // 是否使用 sRGB 色彩空间

        // 边界颜色（仅 ClampToBorder 时使用）
        float border_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        // 各向异性过滤级别（0 = 禁用）
        float anisotropy = 0.0f;
    };

    // 默认预设
    namespace texture_presets
    {
        // ============================================================================
        // 材质贴图预设
        // ============================================================================

        // 标准漫反射/albedo 贴图（sRGB，带 mipmap）
        inline TextureSpecification Diffuse()
        {
            TextureSpecification spec;
            spec.srgb = true;
            spec.internal_format = TextureInternalFormat::SRGB8_ALPHA8;
            spec.anisotropy = 16.0f;
            return spec;
        }

        // 法线贴图（线性空间，不需要 sRGB）
        inline TextureSpecification Normal()
        {
            TextureSpecification spec;
            spec.srgb = false;
            spec.internal_format = TextureInternalFormat::RGBA8;
            spec.anisotropy = 16.0f;
            return spec;
        }

        // ============================================================================
        // HDR 预设（命名清晰，无冗余别名）
        // ============================================================================

        // HDR 等距矩形贴图（用于加载 .hdr/.exr，然后转换为 cubemap）
        // 用途：LoadFromEquirectangular 内部使用
        inline TextureSpecification HDREquirect()
        {
            TextureSpecification spec;
            spec.internal_format = TextureInternalFormat::RGB16F;
            spec.format = TextureFormat::RGB;
            spec.data_type = TextureDataType::Float;
            spec.wrap_s = TextureWrap::Repeat;      // 水平方向可 repeat（0° = 360°）
            spec.wrap_t = TextureWrap::ClampToEdge; // 垂直方向 clamp（极点）
            spec.wrap_r = TextureWrap::ClampToEdge;
            spec.min_filter = TextureFilter::Linear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = false;
            return spec;
        }

        // HDR Cubemap（用于天空盒渲染、环境反射、IBL）
        // 用途：TextureCubemap 最终输出格式
        inline TextureSpecification HDRCubemap()
        {
            TextureSpecification spec;
            spec.internal_format = TextureInternalFormat::RGB16F;
            spec.format = TextureFormat::RGB;
            spec.data_type = TextureDataType::Float;
            spec.wrap_s = TextureWrap::ClampToEdge;
            spec.wrap_t = TextureWrap::ClampToEdge;
            spec.wrap_r = TextureWrap::ClampToEdge;
            spec.min_filter = TextureFilter::LinearMipmapLinear; // mipmap 用于粗糙度 LOD
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = true;
            return spec;
        }

        // HDR Framebuffer 颜色附件（用于 HDR 离屏渲染）
        // 用途：后处理前的 HDR 渲染目标
        inline TextureSpecification HDRFramebuffer()
        {
            TextureSpecification spec;
            spec.internal_format = TextureInternalFormat::RGBA16F;
            spec.format = TextureFormat::RGBA;
            spec.data_type = TextureDataType::Float;
            spec.wrap_s = TextureWrap::ClampToEdge;
            spec.wrap_t = TextureWrap::ClampToEdge;
            spec.min_filter = TextureFilter::Linear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = false;
            spec.flip_vertically = false;
            return spec;
        }

        // ============================================================================
        // 其他预设
        // ============================================================================

        // 最近邻过滤（像素风格）
        inline TextureSpecification Pixelated()
        {
            TextureSpecification spec;
            spec.min_filter = TextureFilter::Nearest;
            spec.mag_filter = TextureFilter::Nearest;
            spec.generate_mipmaps = false;
            return spec;
        }

        // UI 纹理（无 mipmap，线性过滤）
        inline TextureSpecification UI()
        {
            TextureSpecification spec;
            spec.min_filter = TextureFilter::Linear;
            spec.mag_filter = TextureFilter::Linear;
            spec.generate_mipmaps = false;
            spec.wrap_s = TextureWrap::ClampToEdge;
            spec.wrap_t = TextureWrap::ClampToEdge;
            return spec;
        }
    }

} // namespace zk_pbr::gfx

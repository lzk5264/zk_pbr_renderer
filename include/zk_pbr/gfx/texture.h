#pragma once

#include <stdexcept>
#include <string>

#include <glad/glad.h>

namespace zk_pbr::gfx
{

    // 纹理异常
    struct TextureException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // ============================================================================
    // HDR 图像加载器 (stb_image / tinyexr)
    // ============================================================================

    struct HDRImageData
    {
        float *pixels = nullptr;
        int width = 0;
        int height = 0;
        int channels = 0;

        // 禁止拷贝
        HDRImageData() = default;
        HDRImageData(const HDRImageData &) = delete;
        HDRImageData &operator=(const HDRImageData &) = delete;

        // 允许移动
        HDRImageData(HDRImageData &&other) noexcept;
        HDRImageData &operator=(HDRImageData &&other) noexcept;

        ~HDRImageData();

        bool IsValid() const { return pixels != nullptr && width > 0 && height > 0; }
    };

    // 加载 HDR 图像 (.hdr 或 .exr)
    HDRImageData LoadHDRImage(const std::string &path, bool flip_vertically = true);

} // namespace zk_pbr::gfx

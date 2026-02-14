#pragma once
#include <stdexcept>
#include <string>
#include <cstdint>

namespace zk_pbr::gfx
{
    // 图像加载异常
    struct ImageLoadException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // ============================================================================
    // HDR 图像数据 (float 像素，用于 IBL 环境贴图)
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
    // ============================================================================
    // LDR 图像数据 (uint8 像素，用于材质贴图)
    // ============================================================================
    struct LDRImageData
    {
        uint8_t *pixels = nullptr;
        int width = 0;
        int height = 0;
        int channels = 0; // 1=灰度, 3=RGB, 4=RGBA

        // 禁止拷贝
        LDRImageData() = default;
        LDRImageData(const LDRImageData &) = delete;
        LDRImageData &operator=(const LDRImageData &) = delete;

        // 允许移动
        LDRImageData(LDRImageData &&other) noexcept;
        LDRImageData &operator=(LDRImageData &&other) noexcept;

        ~LDRImageData();

        bool IsValid() const { return pixels != nullptr && width > 0 && height > 0; }
    };

    // ============================================================================
    // 图像加载函数
    // ============================================================================

    // 加载 HDR 图像 (.hdr 或 .exr)
    // @param path: 文件路径
    // @param flip_vertically: 是否垂直翻转 (OpenGL 纹理坐标原点在左下)
    HDRImageData LoadHDRImage(const std::string &path, bool flip_vertically = true);

    // 加载 LDR 图像 (.png, .jpg, .bmp, .tga 等)
    // @param path: 文件路径
    // @param flip_vertically: 是否垂直翻转
    // @param desired_channels: 期望的通道数 (0=自动, 1=灰度, 3=RGB, 4=RGBA)
    LDRImageData LoadLDRImage(const std::string &path, bool flip_vertically = true, int desired_channels = 0);

} // namespace zk_pbr::gfx

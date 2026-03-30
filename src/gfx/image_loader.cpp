#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYEXR_USE_MINIZ 1
#define TINYEXR_USE_THREAD 0
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

#include <zk_pbr/gfx/image_loader.h>

#include <cstring>
#include <vector>
#include <utility>

namespace zk_pbr::gfx
{

    // ========== HDRImageData 实现 ==========

    HDRImageData::HDRImageData(HDRImageData &&other) noexcept
        : pixels(std::exchange(other.pixels, nullptr)),
          width(std::exchange(other.width, 0)),
          height(std::exchange(other.height, 0)),
          channels(std::exchange(other.channels, 0))
    {
    }

    HDRImageData &HDRImageData::operator=(HDRImageData &&other) noexcept
    {
        if (this != &other)
        {
            if (pixels)
            {
                free(pixels);
            }

            pixels = std::exchange(other.pixels, nullptr);
            width = std::exchange(other.width, 0);
            height = std::exchange(other.height, 0);
            channels = std::exchange(other.channels, 0);
        }
        return *this;
    }

    HDRImageData::~HDRImageData()
    {
        if (pixels)
        {
            free(pixels);
        }
    }

    // ========== LDRImageData 实现 ==========

    LDRImageData::LDRImageData(LDRImageData &&other) noexcept
        : pixels(std::exchange(other.pixels, nullptr)),
          width(std::exchange(other.width, 0)),
          height(std::exchange(other.height, 0)),
          channels(std::exchange(other.channels, 0))
    {
    }

    LDRImageData &LDRImageData::operator=(LDRImageData &&other) noexcept
    {
        if (this != &other)
        {
            if (pixels)
            {
                stbi_image_free(pixels);
            }

            pixels = std::exchange(other.pixels, nullptr);
            width = std::exchange(other.width, 0);
            height = std::exchange(other.height, 0);
            channels = std::exchange(other.channels, 0);
        }
        return *this;
    }

    LDRImageData::~LDRImageData()
    {
        if (pixels)
        {
            stbi_image_free(pixels);
        }
    }

    // ========== HDR 图像加载 ==========

    HDRImageData LoadHDRImage(const std::string &path, bool flip_vertically)
    {
        HDRImageData data;

        bool is_exr = (path.size() >= 4 &&
                       (path.substr(path.size() - 4) == ".exr" ||
                        path.substr(path.size() - 4) == ".EXR"));

        if (is_exr)
        {
            // 使用 tinyexr 加载 EXR
            const char *err = nullptr;
            int ret = LoadEXR(&data.pixels, &data.width, &data.height, path.c_str(), &err);

            if (ret != TINYEXR_SUCCESS)
            {
                std::string error_msg = err ? err : "Unknown error";
                if (err)
                    FreeEXRErrorMessage(err);
                throw ImageLoadException("Failed to load EXR: " + path + "\n" + error_msg);
            }

            data.channels = 4; // EXR 总是加载为 RGBA

            // 翻转图像
            if (flip_vertically)
            {
                int row_size = data.width * data.channels;
                std::vector<float> temp_row(row_size);
                for (int y = 0; y < data.height / 2; ++y)
                {
                    float *row1 = data.pixels + y * row_size;
                    float *row2 = data.pixels + (data.height - 1 - y) * row_size;
                    std::memcpy(temp_row.data(), row1, row_size * sizeof(float));
                    std::memcpy(row1, row2, row_size * sizeof(float));
                    std::memcpy(row2, temp_row.data(), row_size * sizeof(float));
                }
            }
        }
        else
        {
            // 使用 stb_image 加载 HDR
            stbi_set_flip_vertically_on_load(flip_vertically);

            data.pixels = stbi_loadf(path.c_str(), &data.width, &data.height, &data.channels, 0);

            if (!data.pixels)
            {
                throw ImageLoadException("Failed to load HDR: " + path + "\n" + stbi_failure_reason());
            }
        }

        return data;
    }

    // ========== LDR 图像加载 ==========

    LDRImageData LoadLDRImage(const std::string &path, bool flip_vertically, int desired_channels)
    {
        LDRImageData data;

        stbi_set_flip_vertically_on_load(flip_vertically);

        data.pixels = stbi_load(path.c_str(), &data.width, &data.height, &data.channels, desired_channels);

        if (!data.pixels)
        {
            throw ImageLoadException("Failed to load image: " + path + "\n" + stbi_failure_reason());
        }

        // 如果指定了 desired_channels，更新 channels
        if (desired_channels > 0)
        {
            data.channels = desired_channels;
        }

        return data;
    }

} // namespace zk_pbr::gfx

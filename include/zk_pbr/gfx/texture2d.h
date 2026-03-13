#pragma once

#include <cstdint>
#include <string>

#include <glad/glad.h>

#include <zk_pbr/gfx/texture_parameters.h>
#include <zk_pbr/gfx/image_loader.h>

namespace zk_pbr::gfx
{

    // 2D 纹理 (RAII)
    // 用于 HDR FBO 颜色附件
    class Texture2D
    {
    public:
        Texture2D() = default;

        // 创建空纹理 (用于 FBO 附件)
        Texture2D(int width, int height, const TextureSpec &spec);

        // 从给定文件路径加载 LDR 图像数据（如 .png, .jpg），创建对应的 2D 纹理。
        static Texture2D LoadFromFile(const std::string &path, const TextureSpec &spec);

        ~Texture2D();

        // 禁止拷贝，允许移动
        Texture2D(const Texture2D &) = delete;
        Texture2D &operator=(const Texture2D &) = delete;
        Texture2D(Texture2D &&other) noexcept;
        Texture2D &operator=(Texture2D &&other) noexcept;

        // 创建 1×1 纯色纹理，用于材质缺图时的默认回退
        // r/g/b/a 范围 [0, 255]
        static Texture2D CreateSolid(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

        static Texture2D ComputeDFG(
            int size = 512,
            int sample_count = 1024,
            bool is_multiscatter = true,
            const TextureSpec &spec = TexturePresets::DFGLUT());

        void Bind(unsigned int slot = 0) const;

        GLuint GetID() const { return id_; }
        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }
        bool IsValid() const { return id_ != 0; }

    private:
        GLuint id_ = 0;
        int width_ = 0;
        int height_ = 0;
        TextureSpec spec_{};

        // 一般用于构造函数，后续可能用于其他改变纹理参数的函数中
        // 基于 当前持有的 spec, 对texture对象所持有texture资源进行参数设置
        void ApplyParameters();
    };

} // namespace zk_pbr::gfx

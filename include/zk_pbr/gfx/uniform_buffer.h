#pragma once

#include <glad/glad.h>
#include <stdexcept>
#include <utility>

namespace zk_pbr::gfx
{

    struct UniformBufferException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Uniform Buffer Object (UBO) 封装
    // 用于高效共享 uniform 数据（如相机矩阵）
    class UniformBuffer
    {
    public:
        UniformBuffer() = default;

        // 创建 UBO
        // @param size: 缓冲区大小（字节）
        // @param usage: GL_STATIC_DRAW, GL_DYNAMIC_DRAW 等
        explicit UniformBuffer(size_t size, GLenum usage = GL_DYNAMIC_DRAW);

        UniformBuffer(const UniformBuffer &) = delete;
        UniformBuffer &operator=(const UniformBuffer &) = delete;

        UniformBuffer(UniformBuffer &&other) noexcept;
        UniformBuffer &operator=(UniformBuffer &&other) noexcept;

        ~UniformBuffer() noexcept;

        // 更新数据
        // @param data: 数据指针
        // @param size: 数据大小（字节）
        // @param offset: 偏移量（字节）
        void SetData(const void *data, size_t size, size_t offset = 0) const noexcept;

        // 绑定到 binding point
        // @param bindingPoint: Binding point (0-based)
        void BindToPoint(GLuint bindingPoint) const noexcept;

        // 绑定到 binding point 的一部分（范围绑定）
        void BindRangeToPoint(GLuint bindingPoint, size_t offset, size_t size) const noexcept;

        [[nodiscard]] GLuint GetID() const noexcept { return ubo_; }
        [[nodiscard]] size_t GetSize() const noexcept { return size_; }
        [[nodiscard]] bool IsValid() const noexcept { return ubo_ != 0; }

    private:
        GLuint ubo_ = 0;
        size_t size_ = 0;

        void Cleanup() noexcept;
    };

} // namespace zk_pbr::gfx

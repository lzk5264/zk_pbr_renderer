#include <zk_pbr/gfx/uniform_buffer.h>

namespace zk_pbr::gfx
{

    UniformBuffer::UniformBuffer(size_t size, GLenum usage) : size_(size)
    {
        if (size == 0)
        {
            throw UniformBufferException("UniformBuffer size must be > 0");
        }

        glGenBuffers(1, &ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferData(GL_UNIFORM_BUFFER, size, nullptr, usage);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    UniformBuffer::UniformBuffer(UniformBuffer &&other) noexcept
        : ubo_(std::exchange(other.ubo_, 0)),
          size_(std::exchange(other.size_, 0))
    {
    }

    UniformBuffer &UniformBuffer::operator=(UniformBuffer &&other) noexcept
    {
        if (this != &other)
        {
            Cleanup();
            ubo_ = std::exchange(other.ubo_, 0);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    UniformBuffer::~UniformBuffer() noexcept
    {
        Cleanup();
    }

    void UniformBuffer::SetData(const void *data, size_t size, size_t offset) const noexcept
    {
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void UniformBuffer::BindToPoint(GLuint bindingPoint) const noexcept
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo_);
    }

    void UniformBuffer::BindRangeToPoint(GLuint bindingPoint, size_t offset, size_t size) const noexcept
    {
        glBindBufferRange(GL_UNIFORM_BUFFER, bindingPoint, ubo_, offset, size);
    }

    void UniformBuffer::Cleanup() noexcept
    {
        if (ubo_)
        {
            glDeleteBuffers(1, &ubo_);
            ubo_ = 0;
            size_ = 0;
        }
    }

} // namespace zk_pbr::gfx

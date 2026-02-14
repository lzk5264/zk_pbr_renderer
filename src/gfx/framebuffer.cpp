#include <zk_pbr/gfx/framebuffer.h>

namespace zk_pbr::gfx
{

    Framebuffer::Framebuffer(int width, int height) : width_(width), height_(height)
    {
        if (width <= 0 || height <= 0)
        {
            throw FramebufferException("Framebuffer dimensions must be > 0");
        }

        glGenFramebuffers(1, &fbo_);
    }

    Framebuffer::Framebuffer(Framebuffer &&other) noexcept
        : fbo_(std::exchange(other.fbo_, 0)),
          rbo_(std::exchange(other.rbo_, 0)),
          width_(std::exchange(other.width_, 0)),
          height_(std::exchange(other.height_, 0))
    {
    }

    Framebuffer &Framebuffer::operator=(Framebuffer &&other) noexcept
    {
        if (this != &other)
        {
            Cleanup();
            fbo_ = std::exchange(other.fbo_, 0);
            rbo_ = std::exchange(other.rbo_, 0);
            width_ = std::exchange(other.width_, 0);
            height_ = std::exchange(other.height_, 0);
        }
        return *this;
    }

    Framebuffer::~Framebuffer() noexcept
    {
        Cleanup();
    }

    void Framebuffer::Bind() const noexcept
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    }

    void Framebuffer::Unbind() const noexcept
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::AttachTexture(GLuint texture, GLenum attachment) const noexcept
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, 0);
    }

    void Framebuffer::AttachCubemapFace(GLuint texture, int face, GLenum attachment, int mip_level) const noexcept
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                               texture, mip_level);
    }

    void Framebuffer::AttachDepthTexture(GLuint texture) const noexcept
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
    }

    void Framebuffer::AttachDepthCubemapFace(GLuint texture, int face) const noexcept
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                               texture, 0);
    }

    void Framebuffer::AttachRenderbuffer(GLenum format)
    {
        // 如果已有 renderbuffer，先删除
        CleanupRenderbuffer();

        glGenRenderbuffers(1, &rbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, format, width_, height_);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        if (format == GL_DEPTH_COMPONENT || format == GL_DEPTH_COMPONENT16 ||
            format == GL_DEPTH_COMPONENT24 || format == GL_DEPTH_COMPONENT32F)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_);
        }
        else if (format == GL_DEPTH24_STENCIL8 || format == GL_DEPTH32F_STENCIL8)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_);
        }
    }

    void Framebuffer::CheckStatus() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::string error = "Framebuffer not complete: ";
            switch (status)
            {
            case GL_FRAMEBUFFER_UNDEFINED:
                error += "GL_FRAMEBUFFER_UNDEFINED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                error += "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                error += "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                error += "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                error += "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                error += "GL_FRAMEBUFFER_UNSUPPORTED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                error += "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                error += "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
                break;
            default:
                error += "Unknown error";
                break;
            }
            throw FramebufferException(error);
        }
    }

    void Framebuffer::Resize(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            throw FramebufferException("Framebuffer dimensions must be > 0");
        }

        width_ = width;
        height_ = height;

        // 如果有 renderbuffer，重新创建
        if (rbo_ != 0)
        {
            GLint format;
            glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);

            CleanupRenderbuffer();
            AttachRenderbuffer(static_cast<GLenum>(format));
        }
    }

    void Framebuffer::Cleanup() noexcept
    {
        CleanupRenderbuffer();

        if (fbo_)
        {
            glDeleteFramebuffers(1, &fbo_);
            fbo_ = 0;
        }

        width_ = 0;
        height_ = 0;
    }

    void Framebuffer::CleanupRenderbuffer() noexcept
    {
        if (rbo_)
        {
            glDeleteRenderbuffers(1, &rbo_);
            rbo_ = 0;
        }
    }

} // namespace zk_pbr::gfx

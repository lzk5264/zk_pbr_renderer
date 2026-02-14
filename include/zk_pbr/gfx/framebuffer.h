#pragma once

#include <glad/glad.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace zk_pbr::gfx
{

    struct FramebufferException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Framebuffer Object (FBO) 封装
    // 用于离屏渲染、后处理、阴影贴图等
    class Framebuffer
    {
    public:
        Framebuffer() = default;

        // 创建 FBO
        Framebuffer(int width, int height);

        Framebuffer(const Framebuffer &) = delete;
        Framebuffer &operator=(const Framebuffer &) = delete;

        Framebuffer(Framebuffer &&other) noexcept;
        Framebuffer &operator=(Framebuffer &&other) noexcept;

        ~Framebuffer() noexcept;

        // 绑定/解绑
        void Bind() const noexcept;
        void Unbind() const noexcept;

        // 附加纹理作为颜色附件
        // @param texture: 2D 纹理 ID
        // @param attachment: GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, ...
        void AttachTexture(GLuint texture, GLenum attachment = GL_COLOR_ATTACHMENT0) const noexcept;

        // 附加 Cubemap 某一面作为颜色附件
        // @param texture: Cubemap 纹理 ID
        // @param face: GL_TEXTURE_CUBE_MAP_POSITIVE_X, ... (0-5)
        // @param attachment: GL_COLOR_ATTACHMENT0, ...
        void AttachCubemapFace(GLuint texture, int face, GLenum attachment = GL_COLOR_ATTACHMENT0, int mip_level = 0) const noexcept;

        // 附加深度纹理
        void AttachDepthTexture(GLuint texture) const noexcept;

        // 附加深度立方体贴图某一面
        void AttachDepthCubemapFace(GLuint texture, int face) const noexcept;

        // 创建并附加 Renderbuffer（用于深度/模板缓冲）
        // @param format: GL_DEPTH_COMPONENT, GL_DEPTH24_STENCIL8, etc.
        void AttachRenderbuffer(GLenum format);

        // 检查 FBO 完整性
        // @throws FramebufferException 如果 FBO 不完整
        void CheckStatus() const;

        // 调整大小（会重新创建 renderbuffer）
        void Resize(int width, int height);

        [[nodiscard]] GLuint GetID() const noexcept { return fbo_; }
        [[nodiscard]] GLuint GetRenderbuffer() const noexcept { return rbo_; }
        [[nodiscard]] int GetWidth() const noexcept { return width_; }
        [[nodiscard]] int GetHeight() const noexcept { return height_; }
        [[nodiscard]] bool IsValid() const noexcept { return fbo_ != 0; }

    private:
        GLuint fbo_ = 0;
        GLuint rbo_ = 0; // 可选的 renderbuffer
        int width_ = 0;
        int height_ = 0;

        void Cleanup() noexcept;
        void CleanupRenderbuffer() noexcept;
    };

} // namespace zk_pbr::gfx

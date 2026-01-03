#pragma once

#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace zk_pbr::core
{

    // 轻量级窗口封装
    // 职责：GLFW 初始化、窗口创建、RAII 资源管理
    // 不包含：事件系统、Layer 系统、渲染循环管理
    class Window
    {
    public:
        // 窗口配置
        struct Config
        {
            int width = 800;
            int height = 600;
            std::string title = "ZK PBR Renderer";
            int gl_major_version = 4;
            int gl_minor_version = 6;
            bool resizable = true;
            bool vsync = true;
        };

        // 构造函数：初始化 GLFW 并创建窗口
        explicit Window(const Config &config = Config{});

        // 禁止拷贝
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        // 允许移动
        Window(Window &&other) noexcept;
        Window &operator=(Window &&other) noexcept;

        // 析构：自动清理 GLFW
        ~Window();

        // 窗口控制
        [[nodiscard]] bool ShouldClose() const noexcept;
        void SetShouldClose(bool value) noexcept;
        void PollEvents() const noexcept;
        void SwapBuffers() const noexcept;

        // 窗口属性
        [[nodiscard]] int GetWidth() const noexcept { return width_; }
        [[nodiscard]] int GetHeight() const noexcept { return height_; }
        [[nodiscard]] float GetAspectRatio() const noexcept;
        [[nodiscard]] const std::string &GetTitle() const noexcept { return title_; }

        // 获取原始 GLFW 窗口指针（保持灵活性）
        [[nodiscard]] GLFWwindow *GetNativeWindow() const noexcept { return window_; }

        // 回调设置（委托给 GLFW）
        void SetFramebufferSizeCallback(GLFWframebuffersizefun callback) const noexcept;
        void SetScrollCallback(GLFWscrollfun callback) const noexcept;
        void SetKeyCallback(GLFWkeyfun callback) const noexcept;
        void SetMouseButtonCallback(GLFWmousebuttonfun callback) const noexcept;
        void SetCursorPosCallback(GLFWcursorposfun callback) const noexcept;

        // 用户数据（用于回调中访问 Window 实例）
        void SetUserPointer(void *pointer) const noexcept;
        [[nodiscard]] void *GetUserPointer() const noexcept;

    private:
        GLFWwindow *window_ = nullptr;
        int width_ = 0;
        int height_ = 0;
        std::string title_;

        void InitGLFW(const Config &config);
        void CreateWindow(const Config &config);
        void InitOpenGL();
        void Cleanup() noexcept;
    };

} // namespace zk_pbr::core

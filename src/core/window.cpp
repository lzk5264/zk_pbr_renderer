#include <zk_pbr/core/window.h>

#include <stdexcept>
#include <iostream>

namespace zk_pbr::core
{

    Window::Window(const Config &config)
        : width_(config.width), height_(config.height), title_(config.title)
    {
        InitGLFW(config);
        CreateWindow(config);
        InitOpenGL();
    }

    Window::Window(Window &&other) noexcept
        : window_(other.window_),
          width_(other.width_),
          height_(other.height_),
          title_(std::move(other.title_))
    {
        other.window_ = nullptr;
        other.width_ = 0;
        other.height_ = 0;
    }

    Window &Window::operator=(Window &&other) noexcept
    {
        if (this != &other)
        {
            Cleanup();

            window_ = other.window_;
            width_ = other.width_;
            height_ = other.height_;
            title_ = std::move(other.title_);

            other.window_ = nullptr;
            other.width_ = 0;
            other.height_ = 0;
        }
        return *this;
    }

    Window::~Window()
    {
        Cleanup();
    }

    bool Window::ShouldClose() const noexcept
    {
        return window_ && glfwWindowShouldClose(window_);
    }

    void Window::SetShouldClose(bool value) noexcept
    {
        if (window_)
        {
            glfwSetWindowShouldClose(window_, value ? GLFW_TRUE : GLFW_FALSE);
        }
    }

    void Window::PollEvents() const noexcept
    {
        glfwPollEvents();
    }

    void Window::SwapBuffers() const noexcept
    {
        if (window_)
        {
            glfwSwapBuffers(window_);
        }
    }

    float Window::GetAspectRatio() const noexcept
    {
        return height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
    }

    void Window::SetFramebufferSizeCallback(GLFWframebuffersizefun callback) const noexcept
    {
        if (window_)
        {
            glfwSetFramebufferSizeCallback(window_, callback);
        }
    }

    void Window::SetScrollCallback(GLFWscrollfun callback) const noexcept
    {
        if (window_)
        {
            glfwSetScrollCallback(window_, callback);
        }
    }

    void Window::SetKeyCallback(GLFWkeyfun callback) const noexcept
    {
        if (window_)
        {
            glfwSetKeyCallback(window_, callback);
        }
    }

    void Window::SetMouseButtonCallback(GLFWmousebuttonfun callback) const noexcept
    {
        if (window_)
        {
            glfwSetMouseButtonCallback(window_, callback);
        }
    }

    void Window::SetCursorPosCallback(GLFWcursorposfun callback) const noexcept
    {
        if (window_)
        {
            glfwSetCursorPosCallback(window_, callback);
        }
    }

    void Window::SetUserPointer(void *pointer) const noexcept
    {
        if (window_)
        {
            glfwSetWindowUserPointer(window_, pointer);
        }
    }

    void *Window::GetUserPointer() const noexcept
    {
        return window_ ? glfwGetWindowUserPointer(window_) : nullptr;
    }

    void Window::InitGLFW(const Config &config)
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // 设置 OpenGL 版本
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.gl_major_version);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.gl_minor_version);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // 窗口属性
        glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    }

    void Window::CreateWindow(const Config &config)
    {
        window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
        if (!window_)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(window_);

        // 设置 VSync
        glfwSwapInterval(config.vsync ? 1 : 0);

        std::cout << "Window created: " << width_ << "x" << height_
                  << " (" << title_ << ")" << std::endl;
    }

    void Window::InitOpenGL()
    {
        // 加载 OpenGL 函数指针
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            Cleanup();
            throw std::runtime_error("Failed to initialize GLAD");
        }

        // 打印 OpenGL 信息
        std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    }

    void Window::Cleanup() noexcept
    {
        if (window_)
        {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        glfwTerminate();
    }

} // namespace zk_pbr::core

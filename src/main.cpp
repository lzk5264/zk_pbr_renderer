#include <iostream>
#include <memory>

// Include GLAD before GLFW to avoid OpenGL header conflicts.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/mesh.h>

constexpr unsigned int kScrWidth = 800;
constexpr unsigned int kScrHeight = 600;

void FramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void ProcessInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main()
{
    // glfw window init
    glfwInit();
    // OpenGL context version 4.6, core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // create a window object
    GLFWwindow *window = glfwCreateWindow(kScrWidth, kScrHeight, "zeekang-LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // GLAD init
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    zk_pbr::gfx::Shader shader("./shaders/common/default_screen_space_vs.vert", "./shaders/common/default_screen_space_fs.frag");

    // 使用新的 Mesh 封装 - 方式1：使用 PrimitiveFactory
    auto triangle = zk_pbr::gfx::PrimitiveFactory::CreateTriangle();

    // 或者方式2：手动创建 Mesh（展示灵活性）
    // float vertices[] = {
    //     -0.5f, -0.5f, 0.0f,
    //     0.5f, -0.5f, 0.0f,
    //     0.0f, 0.5f, 0.0f};
    // auto triangle = zk_pbr::gfx::Mesh(vertices, 3, zk_pbr::gfx::layouts::Position3D());

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        ProcessInput(window);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.Use();
        triangle.Draw(); // 使用 Mesh 的 Draw 方法

        // double buffer
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Mesh 会在析构时自动清理 VAO/VBO/EBO
    glfwTerminate();
    return 0;
}
#include <iostream>
#include <memory>

#include <zk_pbr/core/window.h>
#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/mesh.h>
#include <zk_pbr/gfx/texture2d.h>
#include <zk_pbr/gfx/texture_cubemap.h>
#include <zk_pbr/gfx/camera.h>
#include <zk_pbr/gfx/camera_controller.h>
#include <zk_pbr/gfx/uniform_buffer.h>

// 全局相机控制器指针（用于鼠标滚轮回调）
zk_pbr::gfx::ICameraController *g_camera_controller = nullptr;

void FramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (g_camera_controller)
    {
        g_camera_controller->ProcessMouseScroll(static_cast<float>(yoffset));
    }
}

void ProcessInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main()
{
    try
    {
        // 创建窗口（GLFW 初始化 + OpenGL 上下文 + GLAD 加载）
        zk_pbr::core::Window::Config window_config;
        window_config.width = 800;
        window_config.height = 600;
        window_config.title = "ZK PBR Renderer";
        zk_pbr::core::Window window(window_config);

        // 设置回调
        window.SetFramebufferSizeCallback(FramebufferSizeCallback);
        window.SetScrollCallback(ScrollCallback);

        // 启用深度测试
        glEnable(GL_DEPTH_TEST);

        // 创建相机和控制器
        zk_pbr::gfx::Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
        zk_pbr::gfx::EditorCameraController editor_controller(camera);

        // 设置全局控制器指针（用于滚轮回调）
        g_camera_controller = &editor_controller;

        std::cout << "\n=== Camera Controls ===" << std::endl;
        std::cout << "Mode: " << editor_controller.GetName() << std::endl;
        std::cout << "  - Hold RIGHT MOUSE + WASD/QE to move" << std::endl;
        std::cout << "  - Hold MIDDLE MOUSE to pan" << std::endl;
        std::cout << "  - SCROLL to zoom (FOV)" << std::endl;
        std::cout << "  - ESC to exit\n"
                  << std::endl;

        // 加载 Shader
        zk_pbr::gfx::Shader shader(
            "./shaders/common/default_screen_space_vs.vert",
            "./shaders/common/default_screen_space_fs.frag");

        zk_pbr::gfx::Shader skybox_shader(
            "./shaders/common/skybox_vs.vert",
            "./shaders/common/skybox_fs.frag");

        std::array<std::string, 6> faces = {
            "./resources/textures/skybox/right.jpg",  // +X
            "./resources/textures/skybox/left.jpg",   // -X
            "./resources/textures/skybox/top.jpg",    // +Y
            "./resources/textures/skybox/bottom.jpg", // -Y
            "./resources/textures/skybox/front.jpg",  // +Z
            "./resources/textures/skybox/back.jpg"    // -Z
        };

        auto skybox = zk_pbr::gfx::TextureCubemap::LoadFromFiles(faces);

        // 创建几何体
        auto triangle = zk_pbr::gfx::PrimitiveFactory::CreateTriangle();
        auto cube = zk_pbr::gfx::PrimitiveFactory::CreateCube();

        struct CameraMatrices
        {
            glm::mat4 view;       // offset 0,  size 64, align 16 ✅
            glm::mat4 projection; // offset 64, size 64, align 16 ✅
        };
        static_assert(sizeof(CameraMatrices) == 128, "Size check");
        zk_pbr::gfx::UniformBuffer cameraUBO(sizeof(CameraMatrices), GL_DYNAMIC_DRAW);
        cameraUBO.BindToPoint(0);
        skybox_shader.Use();
        skybox_shader.SetUniformBlock("CameraMatrices", 0);

        // 时间管理
        float delta_time = 0.0f;
        float last_frame = 0.0f;

        // 渲染循环
        while (!window.ShouldClose())
        {
            // 计算 delta time
            float current_frame = static_cast<float>(glfwGetTime());
            delta_time = current_frame - last_frame;
            last_frame = current_frame;

            // 更新相机
            editor_controller.Update(window.GetNativeWindow(), delta_time);

            // 输入处理
            ProcessInput(window.GetNativeWindow());

            // 清屏
            glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // 获取相机矩阵
            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 projection = camera.GetProjectionMatrix(window.GetAspectRatio());

            CameraMatrices matrices;
            matrices.view = view;
            matrices.projection = projection;
            cameraUBO.SetData(&matrices, sizeof(matrices));

            // 渲染三角形
            shader.Use();
            shader.SetMat4("view", view);
            shader.SetMat4("projection", projection);
            shader.SetMat4("model", glm::mat4(1.0f));
            triangle.Draw();

            // skybox
            glDepthFunc(GL_LEQUAL);
            skybox_shader.Use();
            // 直接绑定到 binding=0，不需要传 uniform 名字
            zk_pbr::gfx::Shader::BindTextureToUnit(skybox.GetID(), 0, GL_TEXTURE_CUBE_MAP);
            cube.Draw();
            glDepthFunc(GL_LESS);

            window.SwapBuffers();
            window.PollEvents();
        }

        std::cout << "Shutting down..." << std::endl;
        // Window 析构时自动清理 GLFW
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
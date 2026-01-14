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
#include <zk_pbr/gfx/ubo_definitions.h>
#include <zk_pbr/gfx/framebuffer.h>

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
        using namespace zk_pbr;
        // 创建窗口（GLFW 初始化 + OpenGL 上下文 + GLAD 加载）
        zk_pbr::core::Window::Config window_config;
        window_config.width = 1920;
        window_config.height = 1080;
        window_config.title = "ZK PBR Renderer";
        zk_pbr::core::Window window(window_config);

        // 设置回调
        window.SetFramebufferSizeCallback(FramebufferSizeCallback);
        window.SetScrollCallback(ScrollCallback);

        // 启用深度测试
        glEnable(GL_DEPTH_TEST);

        // 启用 Cubemap 无缝过滤（避免面与面交界处接缝）
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

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
        zk_pbr::gfx::Shader debug_default_shader(
            "./shaders/common/debug_default_vs.vert",
            "./shaders/common/debug_default_fs.frag");

        gfx::Shader postprocess_shader(
            "./shaders/common/postprocess_vs.vert",
            "./shaders/common/postprocess_tonemapping_fs.frag");

        gfx::Shader skybox_shader(
            "./shaders/common/skybox_vs.vert",
            "./shaders/common/skybox_fs.frag");

        // 创建几何体
        auto triangle = zk_pbr::gfx::PrimitiveFactory::CreateTriangle();
        auto cube = zk_pbr::gfx::PrimitiveFactory::CreateCube();

        // 创建一个空 VAO 用于后处理全屏绘制（Core Profile 必须要有 VAO 绑定）
        // 使用 RAII 风格管理
        struct EmptyVAO
        {
            GLuint id = 0;
            EmptyVAO() { glGenVertexArrays(1, &id); }
            ~EmptyVAO()
            {
                if (id)
                    glDeleteVertexArrays(1, &id);
            }
            EmptyVAO(const EmptyVAO &) = delete;
            EmptyVAO &operator=(const EmptyVAO &) = delete;
        } empty_vao;

        // UBO（使用公共定义）
        zk_pbr::gfx::UniformBuffer camera_ubo(sizeof(gfx::CameraUBOData), GL_DYNAMIC_DRAW);
        camera_ubo.BindToPoint(gfx::ubo_binding::kCamera);

        gfx::ObjectUBOData object_data;
        object_data.model = glm::mat4(1.0f);
        zk_pbr::gfx::UniformBuffer object_ubo(sizeof(gfx::ObjectUBOData), GL_DYNAMIC_DRAW);
        object_ubo.BindToPoint(gfx::ubo_binding::kObject);

        // FBO（使用配置中的宽高）
        gfx::Framebuffer hdr_fbo(window_config.width, window_config.height);
        gfx::Texture2D hdr_color_texture(window_config.width, window_config.height, gfx::texture_presets::HDRFramebuffer());
        hdr_fbo.AttachTexture(hdr_color_texture.GetID(), GL_COLOR_ATTACHMENT0);
        hdr_fbo.AttachRenderbuffer(GL_DEPTH_COMPONENT24);
        hdr_fbo.CheckStatus();

        // === 预处理：将等距矩形 HDR 转换为 cubemap ===
        const int cubemap_size = 1024;
        auto env_cubemap = gfx::TextureCubemap::LoadFromEquirectangular(
            "./resources/textures/hdr_equirect/je_gray_02_4k.exr",
            cubemap_size,
            gfx::texture_presets::HDRCubemap());

        // === 预处理：从环境 cubemap 生成 irradiance map ===
        auto irradiance_cubemap = gfx::TextureCubemap::ConvolveIrradiance(
            env_cubemap,
            32,   // irradiance map 尺寸（32x32 足够）
            512); // 采样数

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
            hdr_fbo.Bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // 获取相机矩阵
            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 projection = camera.GetProjectionMatrix(window.GetAspectRatio());

            gfx::CameraUBOData camera_data;
            camera_data.view = view;
            camera_data.projection = projection;
            camera_ubo.SetData(&camera_data, sizeof(camera_data));

            // 渲染三角形
            debug_default_shader.Use();
            object_ubo.SetData(&object_data, sizeof(object_data));
            triangle.Draw();

            // skybox
            glDepthFunc(GL_LEQUAL);
            skybox_shader.Use();
            zk_pbr::gfx::Shader::BindTextureToUnit(env_cubemap.GetID(), 0, GL_TEXTURE_CUBE_MAP);
            cube.Draw();
            glDepthFunc(GL_LESS);

            hdr_fbo.Unbind();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            postprocess_shader.Use();
            postprocess_shader.SetFloat("u_Exposure", 1.0f);
            postprocess_shader.SetFloat("u_Gamma", 2.2f);
            hdr_color_texture.Bind(0);

            glBindVertexArray(empty_vao.id);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);

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
#include <iostream>
#include <memory>
#include <vector>
#include <cfloat>
#include <algorithm>

#include <zk_pbr/core/window.h>
#include <zk_pbr/gfx/shader.h>

#include <zk_pbr/gfx/scene_environment.h>
#include <zk_pbr/gfx/mesh.h>
#include <zk_pbr/gfx/material.h>
#include <zk_pbr/gfx/model.h>

#include <zk_pbr/gfx/camera.h>
#include <zk_pbr/gfx/camera_controller.h>

#include <zk_pbr/gfx/uniform_buffer.h>
#include <zk_pbr/gfx/render_constants.h>
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

        // 加载 shader
        gfx::Shader postprocess_shader(
            "./shaders/common/postprocess_vs.vert",
            "./shaders/common/postprocess_tonemapping_fs.frag");

        gfx::Shader skybox_shader(
            "./shaders/common/skybox_vs.vert",
            "./shaders/common/skybox_fs.frag");

        gfx::Shader pbr_shader(
            "./shaders/IBL/pbr_shading_vs.vert",
            "./shaders/IBL/pbr_shading_fs.frag");

        gfx::Shader shadow_shader(
            "./shaders/common/shadow_depth_vs.vert",
            "./shaders/common/shadow_depth_fs.frag");

        gfx::Shader bloom_downsample_shader(
            "./shaders/common/postprocess_vs.vert",
            "./shaders/common/bloom_downsample_fs.frag");

        gfx::Shader bloom_upsample_shader(
            "./shaders/common/postprocess_vs.vert",
            "./shaders/common/bloom_upsample_fs.frag");

        // 创建几何体
        auto cube = zk_pbr::gfx::PrimitiveFactory::CreateCube();
        auto quad = zk_pbr::gfx::PrimitiveFactory::CreateQuad();

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
        object_data.model_inv_t = glm::transpose(glm::inverse(object_data.model));
        zk_pbr::gfx::UniformBuffer object_ubo(sizeof(gfx::ObjectUBOData), GL_DYNAMIC_DRAW);
        object_ubo.BindToPoint(gfx::ubo_binding::kObject);

        // 方向光参数
        glm::vec3 light_dir_raw = glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f));
        glm::vec3 light_color_raw = glm::vec3(3.0f, 2.9f, 2.7f); // 暖白色，强度 ~3

        // 光源 VP 矩阵（正交投影，覆盖场景范围）
        glm::mat4 light_view = glm::lookAt(
            light_dir_raw * 5.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 light_proj = glm::ortho(-3.0f, 3.0f, -3.0f, 3.0f, 0.1f, 15.0f);

        gfx::LightingUBOData lighting_data;
        lighting_data.light_dir = glm::vec4(light_dir_raw, 0.0f);
        lighting_data.light_color = glm::vec4(light_color_raw, 0.0f);
        lighting_data.light_space_matrix = light_proj * light_view;

        zk_pbr::gfx::UniformBuffer lighting_ubo(sizeof(gfx::LightingUBOData), GL_DYNAMIC_DRAW);
        lighting_ubo.BindToPoint(gfx::ubo_binding::kLighting);
        lighting_ubo.SetData(&lighting_data, sizeof(lighting_data));

        // FBO（使用配置中的宽高）
        gfx::Framebuffer hdr_fbo(window_config.width, window_config.height);
        gfx::Texture2D hdr_color_texture(window_config.width, window_config.height, gfx::TexturePresets::HDRRenderTarget());
        hdr_fbo.AttachTexture(hdr_color_texture.GetID(), GL_COLOR_ATTACHMENT0);
        hdr_fbo.AttachRenderbuffer(GL_DEPTH_COMPONENT24);
        hdr_fbo.CheckStatus();

        // ---- Shadow Map ----
        constexpr int kShadowMapSize = 2048;
        gfx::Texture2D shadow_depth_tex(kShadowMapSize, kShadowMapSize, gfx::TexturePresets::ShadowMap());
        // 超出光源视锥的采样返回深度 1.0（无阴影）
        glBindTexture(GL_TEXTURE_2D, shadow_depth_tex.GetID());
        float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
        glBindTexture(GL_TEXTURE_2D, 0);

        gfx::Framebuffer shadow_fbo(kShadowMapSize, kShadowMapSize);
        shadow_fbo.AttachDepthTexture(shadow_depth_tex.GetID());
        // 仅深度，不需要颜色附件
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo.GetID());
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        shadow_fbo.CheckStatus();

        // ---- Bloom Mip Chain ----
        constexpr int kBloomMipCount = 5;
        struct BloomMip
        {
            int width, height;
        };
        std::vector<BloomMip> bloom_mip_sizes;
        std::vector<gfx::Texture2D> bloom_textures;
        std::vector<gfx::Framebuffer> bloom_fbos;

        bloom_mip_sizes.reserve(kBloomMipCount);
        bloom_textures.reserve(kBloomMipCount);
        bloom_fbos.reserve(kBloomMipCount);

        int bw = window_config.width / 2;
        int bh = window_config.height / 2;
        for (int i = 0; i < kBloomMipCount; ++i)
        {
            bloom_mip_sizes.push_back({bw, bh});
            bloom_textures.push_back(gfx::Texture2D(bw, bh, gfx::TexturePresets::BloomMip()));
            bloom_fbos.push_back(gfx::Framebuffer(bw, bh));
            bloom_fbos.back().AttachTexture(bloom_textures.back().GetID(), GL_COLOR_ATTACHMENT0);
            bloom_fbos.back().CheckStatus();
            bw = std::max(1, bw / 2);
            bh = std::max(1, bh / 2);
        }

        // SceneEnvironment 对象，管理环境贴图以及IBL相关资源的管理与计算
        gfx::SceneEnvironment se_object =
            gfx::SceneEnvironment::LoadFromHDR("./resources/textures/hdr_equirect/blue_photo_studio_4k.exr");

        // 导入模型
        gfx::Model model = gfx::Model::LoadFromGLTF("./resources/models/DamagedHelmet/glTF/DamagedHelmet.gltf");

        std::vector<gfx::DrawCommand> model_commands = model.GetDrawCommands();

        // ---- 地面 Quad（用于接收阴影） ----
        // PBRVertex: pos(3) + normal(3) + uv(2) + tangent(4) = 12 floats/vertex
        // Y = -1.0 的水平面，大小 10×10
        constexpr float kGroundY = -1.0f;
        constexpr float kGroundHalf = 5.0f;
        // clang-format off
        float ground_vertices[] = {
            // pos                          normal          uv          tangent (xyz, handedness)
            -kGroundHalf, kGroundY, -kGroundHalf,  0,1,0,  0,0,  1,0,0, 1,
             kGroundHalf, kGroundY, -kGroundHalf,  0,1,0,  1,0,  1,0,0, 1,
             kGroundHalf, kGroundY,  kGroundHalf,  0,1,0,  1,1,  1,0,0, 1,
            -kGroundHalf, kGroundY,  kGroundHalf,  0,1,0,  0,1,  1,0,0, 1,
        };
        unsigned int ground_indices[] = { 0, 1, 2, 0, 2, 3 };
        // clang-format on
        gfx::Mesh ground_mesh(ground_vertices, 4, ground_indices, 6, gfx::layouts::PBRVertex());

        // 地面材质：浅灰色、非金属、中等粗糙度
        gfx::Material ground_material;
        ground_material.base_color_factor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        ground_material.metallic_factor = 0.0f;
        ground_material.roughness_factor = 0.8f;
        ground_material.emissive_factor = glm::vec3(0.0f);

        glm::mat4 ground_model_matrix = glm::mat4(1.0f);
        glm::mat4 ground_model_inv_t = glm::mat4(1.0f); // 无变换，单位矩阵即可

        model_commands.push_back(gfx::DrawCommand{&ground_mesh, &ground_material, ground_model_matrix, ground_model_inv_t});

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

            // 获取相机矩阵
            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 projection = camera.GetProjectionMatrix(window.GetAspectRatio());

            gfx::CameraUBOData camera_data;
            camera_data.view = view;
            camera_data.projection = projection;
            camera_data.SetCameraPos(camera.GetPosition());
            camera_ubo.SetData(&camera_data, sizeof(camera_data));

            // ================================================================
            // Pass 1: Shadow Depth
            // ================================================================
            shadow_fbo.Bind();
            glViewport(0, 0, kShadowMapSize, kShadowMapSize);
            glClear(GL_DEPTH_BUFFER_BIT);

            shadow_shader.Use();
            for (const auto &command : model_commands)
            {
                object_data.model = command.model_matrix;
                object_data.model_inv_t = command.model_inv_t;
                object_ubo.SetData(&object_data, sizeof(object_data));
                command.mesh->Draw();
            }

            // ================================================================
            // Pass 2: PBR Shading + Skybox → HDR FBO
            // ================================================================
            hdr_fbo.Bind();
            glViewport(0, 0, window_config.width, window_config.height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            pbr_shader.Use();
            se_object.BindIBL();
            shadow_depth_tex.Bind(gfx::texture_slot::kShadowMap);
            for (const auto &command : model_commands)
            {
                command.material->Bind();
                object_data.model = command.model_matrix;
                object_data.model_inv_t = command.model_inv_t;
                object_ubo.SetData(&object_data, sizeof(object_data));
                command.mesh->Draw();
            }

            // Skybox
            glDepthFunc(GL_LEQUAL);
            skybox_shader.Use();
            zk_pbr::gfx::Shader::BindTextureToUnit(se_object.GetEnvCubemap().GetID(), 0, GL_TEXTURE_CUBE_MAP);
            cube.Draw();
            glDepthFunc(GL_LESS);

            // ================================================================
            // Pass 3: Bloom (downsample → upsample)
            // ================================================================
            glDisable(GL_DEPTH_TEST);
            glBindVertexArray(empty_vao.id);

            // --- Downsample chain ---
            bloom_downsample_shader.Use();

            // 第一次降采样：从 HDR buffer 读取，做亮度提取 + Karis average
            bloom_fbos[0].Bind();
            glViewport(0, 0, bloom_mip_sizes[0].width, bloom_mip_sizes[0].height);
            hdr_color_texture.Bind(0);
            bloom_downsample_shader.SetVec2("u_SrcTexelSize",
                glm::vec2(1.0f / window_config.width, 1.0f / window_config.height));
            bloom_downsample_shader.SetBool("u_FirstPass", true);
            bloom_downsample_shader.SetFloat("u_Threshold", 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            // 后续降采样：逐级半分辨率
            bloom_downsample_shader.SetBool("u_FirstPass", false);
            for (int i = 1; i < kBloomMipCount; ++i)
            {
                bloom_fbos[i].Bind();
                glViewport(0, 0, bloom_mip_sizes[i].width, bloom_mip_sizes[i].height);
                bloom_textures[i - 1].Bind(0);
                bloom_downsample_shader.SetVec2("u_SrcTexelSize",
                    glm::vec2(1.0f / bloom_mip_sizes[i - 1].width,
                              1.0f / bloom_mip_sizes[i - 1].height));
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }

            // --- Upsample chain (additive blending) ---
            bloom_upsample_shader.Use();
            bloom_upsample_shader.SetFloat("u_BloomIntensity", 1.0f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);

            for (int i = kBloomMipCount - 1; i > 0; --i)
            {
                bloom_fbos[i - 1].Bind();
                glViewport(0, 0, bloom_mip_sizes[i - 1].width, bloom_mip_sizes[i - 1].height);
                bloom_textures[i].Bind(0);
                bloom_upsample_shader.SetVec2("u_SrcTexelSize",
                    glm::vec2(1.0f / bloom_mip_sizes[i].width,
                              1.0f / bloom_mip_sizes[i].height));
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }

            glDisable(GL_BLEND);
            glBindVertexArray(0);

            // ================================================================
            // Pass 4: Tonemapping + Bloom Composite → Default Framebuffer
            // ================================================================
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, window_config.width, window_config.height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            postprocess_shader.Use();
            postprocess_shader.SetFloat("u_Exposure", 1.0f);
            postprocess_shader.SetFloat("u_Gamma", 2.2f);
            postprocess_shader.SetFloat("u_BloomStrength", 0.04f);
            hdr_color_texture.Bind(0);
            bloom_textures[0].Bind(1);

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
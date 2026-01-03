#pragma once

#include <zk_pbr/gfx/camera.h>
#include <GLFW/glfw3.h>

namespace zk_pbr::gfx
{

    // 相机控制器抽象接口
    class ICameraController
    {
    public:
        virtual ~ICameraController() = default;

        // 每帧更新
        virtual void Update(GLFWwindow *window, float delta_time) = 0;

        // 鼠标滚轮事件（用于缩放）
        virtual void ProcessMouseScroll(float yoffset) = 0;

        // 获取控制器名称（用于调试）
        [[nodiscard]] virtual const char *GetName() const noexcept = 0;

        // 参数设置
        virtual void SetMoveSpeed(float speed) noexcept = 0;
        virtual void SetMouseSensitivity(float sensitivity) noexcept = 0;

        [[nodiscard]] virtual float GetMoveSpeed() const noexcept = 0;
        [[nodiscard]] virtual float GetMouseSensitivity() const noexcept = 0;
    };

    // Editor 相机控制器：按住鼠标右键才旋转视角
    // 适合集成 ImGui 的场景，不会误操作 UI
    class EditorCameraController : public ICameraController
    {
    public:
        explicit EditorCameraController(Camera &camera,
                                        float move_speed = 5.0f,
                                        float mouse_sensitivity = 0.1f);

        void Update(GLFWwindow *window, float delta_time) override;
        void ProcessMouseScroll(float yoffset) override;

        [[nodiscard]] const char *GetName() const noexcept override { return "Editor"; }

        void SetMoveSpeed(float speed) noexcept override { move_speed_ = speed; }
        void SetMouseSensitivity(float sensitivity) noexcept override { mouse_sensitivity_ = sensitivity; }

        [[nodiscard]] float GetMoveSpeed() const noexcept override { return move_speed_; }
        [[nodiscard]] float GetMouseSensitivity() const noexcept override { return mouse_sensitivity_; }

        // Editor 特有：平移速度（中键拖拽）
        void SetPanSpeed(float speed) noexcept { pan_speed_ = speed; }
        [[nodiscard]] float GetPanSpeed() const noexcept { return pan_speed_; }

    private:
        Camera &camera_;
        float move_speed_;
        float mouse_sensitivity_;
        float pan_speed_ = 0.01f;

        // 鼠标状态
        bool first_right_click_ = true;
        bool first_middle_click_ = true;
        double last_x_ = 0.0;
        double last_y_ = 0.0;
    };

    // FPS 相机控制器：鼠标直接控制视角（传统 FPS 游戏）
    // 需要隐藏并锁定光标
    class FPSCameraController : public ICameraController
    {
    public:
        explicit FPSCameraController(Camera &camera,
                                     float move_speed = 5.0f,
                                     float mouse_sensitivity = 0.1f);

        void Update(GLFWwindow *window, float delta_time) override;
        void ProcessMouseScroll(float yoffset) override;

        [[nodiscard]] const char *GetName() const noexcept override { return "FPS"; }

        void SetMoveSpeed(float speed) noexcept override { move_speed_ = speed; }
        void SetMouseSensitivity(float sensitivity) noexcept override { mouse_sensitivity_ = sensitivity; }

        [[nodiscard]] float GetMoveSpeed() const noexcept override { return move_speed_; }
        [[nodiscard]] float GetMouseSensitivity() const noexcept override { return mouse_sensitivity_; }

        // FPS 特有：激活/停用（控制鼠标锁定）
        void Activate(GLFWwindow *window);
        void Deactivate(GLFWwindow *window);

    private:
        Camera &camera_;
        float move_speed_;
        float mouse_sensitivity_;

        // 鼠标状态
        bool first_mouse_ = true;
        double last_x_ = 0.0;
        double last_y_ = 0.0;
        bool is_active_ = false;
    };

    // Orbit 相机控制器（可选实现）：围绕目标点旋转
    // 适合模型查看器
    class OrbitCameraController : public ICameraController
    {
    public:
        explicit OrbitCameraController(Camera &camera,
                                       const glm::vec3 &target = glm::vec3(0.0f),
                                       float distance = 5.0f,
                                       float orbit_speed = 0.5f);

        void Update(GLFWwindow *window, float delta_time) override;
        void ProcessMouseScroll(float yoffset) override;

        [[nodiscard]] const char *GetName() const noexcept override { return "Orbit"; }

        void SetMoveSpeed(float speed) noexcept override { orbit_speed_ = speed; }
        void SetMouseSensitivity(float sensitivity) noexcept override { orbit_speed_ = sensitivity; }

        [[nodiscard]] float GetMoveSpeed() const noexcept override { return orbit_speed_; }
        [[nodiscard]] float GetMouseSensitivity() const noexcept override { return orbit_speed_; }

        // Orbit 特有
        void SetTarget(const glm::vec3 &target) noexcept;
        void SetDistance(float distance) noexcept;
        [[nodiscard]] const glm::vec3 &GetTarget() const noexcept { return target_; }
        [[nodiscard]] float GetDistance() const noexcept { return distance_; }

    private:
        Camera &camera_;
        glm::vec3 target_;
        float distance_;
        float orbit_speed_;

        // 轨道角度
        float azimuth_ = 0.0f;    // 水平角度
        float elevation_ = 30.0f; // 垂直角度

        // 鼠标状态
        bool is_rotating_ = false;
        double last_x_ = 0.0;
        double last_y_ = 0.0;

        void UpdateCameraPosition();
    };

} // namespace zk_pbr::gfx

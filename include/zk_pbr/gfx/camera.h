#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace zk_pbr::gfx
{

    // 相机类：纯数据 + 数学计算
    // 不包含输入处理逻辑，由 CameraController 负责
    class Camera
    {
    public:
        // 构造函数
        Camera(const glm::vec3 &position = glm::vec3(0.0f, 0.0f, 3.0f),
               float yaw = -90.0f,
               float pitch = 0.0f,
               float fov = 45.0f);

        // 获取矩阵
        [[nodiscard]] glm::mat4 GetViewMatrix() const noexcept;
        [[nodiscard]] glm::mat4 GetProjectionMatrix(float aspect, float near = 0.1f, float far = 100.0f) const noexcept;

        // 获取向量
        [[nodiscard]] const glm::vec3 &GetPosition() const noexcept { return position_; }
        [[nodiscard]] const glm::vec3 &GetFront() const noexcept { return front_; }
        [[nodiscard]] const glm::vec3 &GetRight() const noexcept { return right_; }
        [[nodiscard]] const glm::vec3 &GetUp() const noexcept { return up_; }

        // 获取角度
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] float GetPitch() const noexcept { return pitch_; }
        [[nodiscard]] float GetFOV() const noexcept { return fov_; }

        // 设置位置
        void SetPosition(const glm::vec3 &position) noexcept;
        void Translate(const glm::vec3 &offset) noexcept;

        // 设置旋转（欧拉角）
        void SetRotation(float yaw, float pitch) noexcept;
        void RotateYaw(float yaw_offset) noexcept;
        void RotatePitch(float pitch_offset) noexcept;

        // 设置 FOV
        void SetFOV(float fov) noexcept;
        void AddFOV(float fov_offset) noexcept; // 用于滚轮缩放

        // 世界坐标系常量
        static constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};

        // FOV 限制
        static constexpr float kMinFOV = 1.0f;
        static constexpr float kMaxFOV = 120.0f;

        // Pitch 限制（防止万向锁）
        static constexpr float kMinPitch = -89.0f;
        static constexpr float kMaxPitch = 89.0f;

    private:
        // 位置和方向
        glm::vec3 position_;
        glm::vec3 front_;
        glm::vec3 right_;
        glm::vec3 up_;

        // 欧拉角
        float yaw_;   // 偏航角（绕Y轴）
        float pitch_; // 俯仰角（绕X轴）

        // 相机参数
        float fov_; // 视野角度

        // 从欧拉角更新向量
        void UpdateVectors() noexcept;
    };

} // namespace zk_pbr::gfx

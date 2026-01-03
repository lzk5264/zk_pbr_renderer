#include <zk_pbr/gfx/camera.h>

#include <algorithm>

namespace zk_pbr::gfx
{

    Camera::Camera(const glm::vec3 &position, float yaw, float pitch, float fov)
        : position_(position), yaw_(yaw), pitch_(pitch), fov_(fov)
    {
        UpdateVectors();
    }

    glm::mat4 Camera::GetViewMatrix() const noexcept
    {
        return glm::lookAt(position_, position_ + front_, up_);
    }

    glm::mat4 Camera::GetProjectionMatrix(float aspect, float near, float far) const noexcept
    {
        return glm::perspective(glm::radians(fov_), aspect, near, far);
    }

    void Camera::SetPosition(const glm::vec3 &position) noexcept
    {
        position_ = position;
    }

    void Camera::Translate(const glm::vec3 &offset) noexcept
    {
        position_ += offset;
    }

    void Camera::SetRotation(float yaw, float pitch) noexcept
    {
        yaw_ = yaw;
        pitch_ = std::clamp(pitch, kMinPitch, kMaxPitch);
        UpdateVectors();
    }

    void Camera::RotateYaw(float yaw_offset) noexcept
    {
        yaw_ += yaw_offset;
        UpdateVectors();
    }

    void Camera::RotatePitch(float pitch_offset) noexcept
    {
        pitch_ += pitch_offset;
        pitch_ = std::clamp(pitch_, kMinPitch, kMaxPitch);
        UpdateVectors();
    }

    void Camera::SetFOV(float fov) noexcept
    {
        fov_ = std::clamp(fov, kMinFOV, kMaxFOV);
    }

    void Camera::AddFOV(float fov_offset) noexcept
    {
        fov_ += fov_offset;
        fov_ = std::clamp(fov_, kMinFOV, kMaxFOV);
    }

    void Camera::UpdateVectors() noexcept
    {
        // 从欧拉角计算新的 front 向量
        glm::vec3 front;
        front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front.y = sin(glm::radians(pitch_));
        front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_ = glm::normalize(front);

        // 重新计算 right 和 up 向量
        right_ = glm::normalize(glm::cross(front_, kWorldUp));
        up_ = glm::normalize(glm::cross(right_, front_));
    }

} // namespace zk_pbr::gfx

#include <zk_pbr/gfx/camera_controller.h>

#include <algorithm>

namespace zk_pbr::gfx
{

    // ========== EditorCameraController 实现 ==========

    EditorCameraController::EditorCameraController(Camera &camera, float move_speed, float mouse_sensitivity)
        : camera_(camera), move_speed_(move_speed), mouse_sensitivity_(mouse_sensitivity)
    {
    }

    void EditorCameraController::Update(GLFWwindow *window, float delta_time)
    {
        // 按住鼠标右键：旋转视角 + WASD 移动
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            if (first_right_click_)
            {
                last_x_ = xpos;
                last_y_ = ypos;
                first_right_click_ = false;
            }

            float xoffset = static_cast<float>(xpos - last_x_);
            float yoffset = static_cast<float>(last_y_ - ypos); // 反转Y轴
            last_x_ = xpos;
            last_y_ = ypos;

            // 旋转相机
            camera_.RotateYaw(xoffset * mouse_sensitivity_);
            camera_.RotatePitch(yoffset * mouse_sensitivity_);

            // WASD 移动（只在按住右键时生效）
            float velocity = move_speed_ * delta_time;

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                camera_.Translate(camera_.GetFront() * velocity);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                camera_.Translate(-camera_.GetFront() * velocity);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                camera_.Translate(-camera_.GetRight() * velocity);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                camera_.Translate(camera_.GetRight() * velocity);

            // Q/E 上下移动
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                camera_.Translate(-Camera::kWorldUp * velocity);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                camera_.Translate(Camera::kWorldUp * velocity);
        }
        else
        {
            first_right_click_ = true; // 松开右键后重置
        }

        // 按住鼠标中键：平移视角（拖拽场景）
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            if (first_middle_click_)
            {
                last_x_ = xpos;
                last_y_ = ypos;
                first_middle_click_ = false;
            }

            float xoffset = static_cast<float>(xpos - last_x_);
            float yoffset = static_cast<float>(ypos - last_y_);
            last_x_ = xpos;
            last_y_ = ypos;

            // 平移相机（右和上方向）
            camera_.Translate(-camera_.GetRight() * xoffset * pan_speed_);
            camera_.Translate(camera_.GetUp() * yoffset * pan_speed_);
        }
        else
        {
            first_middle_click_ = true;
        }
    }

    void EditorCameraController::ProcessMouseScroll(float yoffset)
    {
        // 滚轮调整 FOV（缩放效果）
        camera_.AddFOV(-yoffset);
    }

    // ========== FPSCameraController 实现 ==========

    FPSCameraController::FPSCameraController(Camera &camera, float move_speed, float mouse_sensitivity)
        : camera_(camera), move_speed_(move_speed), mouse_sensitivity_(mouse_sensitivity)
    {
    }

    void FPSCameraController::Activate(GLFWwindow *window)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        first_mouse_ = true;
        is_active_ = true;
    }

    void FPSCameraController::Deactivate(GLFWwindow *window)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        is_active_ = false;
    }

    void FPSCameraController::Update(GLFWwindow *window, float delta_time)
    {
        if (!is_active_)
            return;

        // 鼠标控制视角（始终生效）
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (first_mouse_)
        {
            last_x_ = xpos;
            last_y_ = ypos;
            first_mouse_ = false;
        }

        float xoffset = static_cast<float>(xpos - last_x_);
        float yoffset = static_cast<float>(last_y_ - ypos); // 反转Y轴
        last_x_ = xpos;
        last_y_ = ypos;

        camera_.RotateYaw(xoffset * mouse_sensitivity_);
        camera_.RotatePitch(yoffset * mouse_sensitivity_);

        // WASD 移动
        float velocity = move_speed_ * delta_time;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera_.Translate(camera_.GetFront() * velocity);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera_.Translate(-camera_.GetFront() * velocity);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera_.Translate(-camera_.GetRight() * velocity);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera_.Translate(camera_.GetRight() * velocity);

        // 空格/左Shift 上下移动
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera_.Translate(Camera::kWorldUp * velocity);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camera_.Translate(-Camera::kWorldUp * velocity);
    }

    void FPSCameraController::ProcessMouseScroll(float yoffset)
    {
        // 滚轮调整 FOV（缩放效果）
        camera_.AddFOV(-yoffset);
    }

    // ========== OrbitCameraController 实现 ==========

    OrbitCameraController::OrbitCameraController(Camera &camera, const glm::vec3 &target,
                                                 float distance, float orbit_speed)
        : camera_(camera), target_(target), distance_(distance), orbit_speed_(orbit_speed)
    {
        UpdateCameraPosition();
    }

    void OrbitCameraController::SetTarget(const glm::vec3 &target) noexcept
    {
        target_ = target;
        UpdateCameraPosition();
    }

    void OrbitCameraController::SetDistance(float distance) noexcept
    {
        distance_ = std::max(0.1f, distance); // 防止距离为0
        UpdateCameraPosition();
    }

    void OrbitCameraController::Update(GLFWwindow *window, float delta_time)
    {
        // 按住鼠标左键：旋转
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            if (!is_rotating_)
            {
                last_x_ = xpos;
                last_y_ = ypos;
                is_rotating_ = true;
            }

            float xoffset = static_cast<float>(xpos - last_x_);
            float yoffset = static_cast<float>(ypos - last_y_);
            last_x_ = xpos;
            last_y_ = ypos;

            // 更新角度
            azimuth_ -= xoffset * orbit_speed_;
            elevation_ -= yoffset * orbit_speed_;

            // 限制垂直角度
            elevation_ = std::clamp(elevation_, -89.0f, 89.0f);

            UpdateCameraPosition();
        }
        else
        {
            is_rotating_ = false;
        }

        // 按住鼠标中键：平移目标点
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            static bool first_pan = true;
            static double pan_last_x = 0.0;
            static double pan_last_y = 0.0;

            if (first_pan)
            {
                pan_last_x = xpos;
                pan_last_y = ypos;
                first_pan = false;
            }

            float xoffset = static_cast<float>(xpos - pan_last_x);
            float yoffset = static_cast<float>(ypos - pan_last_y);
            pan_last_x = xpos;
            pan_last_y = ypos;

            float pan_speed = distance_ * 0.001f;
            target_ -= camera_.GetRight() * xoffset * pan_speed;
            target_ += camera_.GetUp() * yoffset * pan_speed;

            UpdateCameraPosition();
        }
    }

    void OrbitCameraController::ProcessMouseScroll(float yoffset)
    {
        // 滚轮调整距离
        distance_ -= yoffset * 0.5f;
        distance_ = std::clamp(distance_, 0.1f, 100.0f);
        UpdateCameraPosition();
    }

    void OrbitCameraController::UpdateCameraPosition()
    {
        // 从球坐标计算相机位置
        float x = target_.x + distance_ * cos(glm::radians(elevation_)) * cos(glm::radians(azimuth_));
        float y = target_.y + distance_ * sin(glm::radians(elevation_));
        float z = target_.z + distance_ * cos(glm::radians(elevation_)) * sin(glm::radians(azimuth_));

        camera_.SetPosition(glm::vec3(x, y, z));

        // 让相机朝向目标点
        glm::vec3 direction = glm::normalize(target_ - camera_.GetPosition());

        // 计算对应的 yaw 和 pitch
        float yaw = glm::degrees(atan2(direction.z, direction.x));
        float pitch = glm::degrees(asin(direction.y));

        camera_.SetRotation(yaw, pitch);
    }

} // namespace zk_pbr::gfx

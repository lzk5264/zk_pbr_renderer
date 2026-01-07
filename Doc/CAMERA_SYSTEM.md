# Camera 系统使用文档

## 概述

相机系统由两部分组成：
- **Camera**：纯数据类，负责存储相机状态和数学计算
- **CameraController**：控制逻辑类，负责处理输入和更新相机

---

## Camera 类

### 核心功能

```cpp
#include <zk_pbr/gfx/camera.h>

// 创建相机
Camera camera(
    glm::vec3(0.0f, 0.0f, 3.0f),  // 位置
    -90.0f,                        // yaw (水平角度)
    0.0f,                          // pitch (俯仰角度)
    45.0f                          // FOV
);

// 获取矩阵（用于 shader）
glm::mat4 view = camera.GetViewMatrix();
glm::mat4 proj = camera.GetProjectionMatrix(aspect_ratio);

// 获取向量（用于计算）
glm::vec3 pos = camera.GetPosition();
glm::vec3 front = camera.GetFront();
glm::vec3 right = camera.GetRight();
glm::vec3 up = camera.GetUp();

// 移动相机
camera.SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
camera.Translate(camera.GetFront() * speed * dt); // 向前移动

// 旋转相机
camera.SetRotation(yaw, pitch);
camera.RotateYaw(10.0f);   // 水平旋转 10 度
camera.RotatePitch(-5.0f); // 向下旋转 5 度

// 调整 FOV
camera.SetFOV(60.0f);
camera.AddFOV(-5.0f); // 减少 5 度（缩放效果）
```

### 自动限制

- **Pitch**：自动限制在 [-89°, 89°] 防止万向锁
- **FOV**：自动限制在 [1°, 120°]

---

## CameraController 系统

### 三种控制器

#### 1. EditorCameraController（推荐用于开发）

**特点**：
- 按住**鼠标右键**才旋转视角（不会误操作 UI）
- 按住**鼠标中键**平移场景
- **滚轮**调整 FOV
- 适合集成 ImGui 的场景

**使用示例**：
```cpp
#include <zk_pbr/gfx/camera_controller.h>

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
EditorCameraController controller(camera);

// 设置参数
controller.SetMoveSpeed(10.0f);
controller.SetMouseSensitivity(0.2f);
controller.SetPanSpeed(0.02f);

// 主循环
while (!glfwWindowShouldClose(window)) {
    float dt = CalculateDeltaTime();
    
    // 更新控制器
    controller.Update(window, dt);
    
    // 使用相机矩阵
    shader.SetMat4("view", camera.GetViewMatrix());
    shader.SetMat4("projection", camera.GetProjectionMatrix(aspect));
    
    Render();
}
```

**操作方式**：
| 操作 | 功能 |
|------|------|
| 按住右键 + 鼠标移动 | 旋转视角 |
| 按住右键 + WASD | 前后左右移动 |
| 按住右键 + Q/E | 上下移动 |
| 按住中键 + 鼠标移动 | 平移场景 |
| 滚轮 | 缩放（调整 FOV）|

---

#### 2. FPSCameraController（传统 FPS）

**特点**：
- 鼠标**直接控制**视角（需要隐藏光标）
- 适合游戏运行时
- 需要显式激活/停用

**使用示例**：
```cpp
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
FPSCameraController controller(camera);

// 激活（隐藏并锁定光标）
controller.Activate(window);

// 主循环
while (!glfwWindowShouldClose(window)) {
    float dt = CalculateDeltaTime();
    controller.Update(window, dt);
    
    // 按 Tab 切换到编辑器模式
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        controller.Deactivate(window);
        // 切换到 EditorController...
    }
    
    Render();
}

// 停用（恢复光标）
controller.Deactivate(window);
```

**操作方式**：
| 操作 | 功能 |
|------|------|
| 鼠标移动 | 旋转视角（始终生效）|
| WASD | 前后左右移动 |
| 空格 | 上升 |
| 左Shift | 下降 |
| 滚轮 | 缩放（调整 FOV）|

---

#### 3. OrbitCameraController（模型查看器）

**特点**：
- 围绕**目标点**旋转
- 适合查看 3D 模型
- 支持平移目标点和缩放距离

**使用示例**：
```cpp
Camera camera;
glm::vec3 model_center(0.0f, 0.0f, 0.0f);
OrbitCameraController controller(camera, model_center, 5.0f);

// 设置参数
controller.SetTarget(model_center);
controller.SetDistance(10.0f);
controller.SetMoveSpeed(0.5f);

// 主循环
while (!glfwWindowShouldClose(window)) {
    float dt = CalculateDeltaTime();
    controller.Update(window, dt);
    
    Render();
}
```

**操作方式**：
| 操作 | 功能 |
|------|------|
| 按住左键 + 鼠标移动 | 旋转（围绕目标）|
| 按住中键 + 鼠标移动 | 平移目标点 |
| 滚轮 | 调整距离 |

---

## 运行时切换控制器

```cpp
// 定义控制器枚举
enum class ControllerMode { Editor, FPS, Orbit };

// 存储当前控制器
std::unique_ptr<ICameraController> controller;

// 切换函数
void SetControllerMode(ControllerMode mode, Camera& camera, GLFWwindow* window) {
    // 停用旧控制器
    if (auto* fps = dynamic_cast<FPSCameraController*>(controller.get())) {
        fps->Deactivate(window);
    }
    
    // 创建新控制器
    switch (mode) {
        case ControllerMode::Editor:
            controller = std::make_unique<EditorCameraController>(camera);
            break;
        case ControllerMode::FPS:
            controller = std::make_unique<FPSCameraController>(camera);
            static_cast<FPSCameraController*>(controller.get())->Activate(window);
            break;
        case ControllerMode::Orbit:
            controller = std::make_unique<OrbitCameraController>(camera);
            break;
    }
}

// 在主循环中
if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
    SetControllerMode(ControllerMode::Editor, camera, window);
}
if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
    SetControllerMode(ControllerMode::FPS, camera, window);
}
```

---



## 完整示例

```cpp
#include <zk_pbr/gfx/camera.h>
#include <zk_pbr/gfx/camera_controller.h>

// 全局变量（用于回调）
ICameraController* g_controller = nullptr;

void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (g_controller) {
        g_controller->ProcessMouseScroll(static_cast<float>(yoffset));
    }
}

int main() {
    // 初始化 GLFW/GLAD...
    
    glfwSetScrollCallback(window, ScrollCallback);
    
    // 创建相机和控制器
    Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
    EditorCameraController controller(camera, 5.0f, 0.1f);
    g_controller = &controller;
    
    // 加载场景...
    
    // 时间管理
    float last_frame = 0.0f;
    
    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        // 计算 delta time
        float current_frame = glfwGetTime();
        float dt = current_frame - last_frame;
        last_frame = current_frame;
        
        // 更新相机
        controller.Update(window, dt);
        
        // 清屏
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 设置矩阵
        float aspect = (float)width / (float)height;
        shader.SetMat4("view", camera.GetViewMatrix());
        shader.SetMat4("projection", camera.GetProjectionMatrix(aspect));
        
        // 渲染场景...
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    return 0;
}
```

---

## 设计说明

### 为什么分离 Camera 和 Controller？

1. **Camera 可以独立使用**：
   - 过场动画（路径插值）
   - 序列化保存
   - 多相机系统（主相机、小地图相机）

2. **Controller 可以热切换**：
   - 编辑器模式 ↔ 游戏模式
   - 第一人称 ↔ 第三人称

3. **逻辑清晰**：
   - Camera：纯数据 + 数学计算（无副作用）
   - Controller：输入处理 + 状态管理

### 为什么 EditorController 更适合开发？

- **不会误操作 UI**：鼠标悬停在 ImGui 上不会旋转视角
- **精确调试**：按住右键时才移动，可以精确定位相机
- **符合习惯**：Unity/Unreal/Blender 都是这种模式

## 常见问题

**Q: 为什么不用四元数？**  
A: 对于 FPS/Editor 相机，欧拉角更直观，且已限制 pitch 避免万向锁。四元数适合第三人称或航天模拟。

**Q: 如何实现第三人称相机？**  
A: 可以基于 OrbitCameraController，添加碰撞检测和距离调整。

**Q: 如何记录相机路径？**  
A: Camera 是纯数据类，直接序列化 position/yaw/pitch/fov 即可。

**Q: 多相机怎么处理？**  
A: 创建多个 Camera 实例，只给主相机绑定 Controller。

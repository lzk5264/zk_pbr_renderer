# Shader UBO 和纹理 Binding 使用指南

## 新增功能

### 1. 纹理 Binding

```cpp
// 方式 1：使用 Shader::SetTexture（推荐）
shader.Use();
shader.SetTexture("skybox", cubemapTexture.GetID(), 0);  // 绑定到纹理单元 0

// 等价于手动操作：
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture.GetID());
shader.SetInt("skybox", 0);
```

### 2. UBO (Uniform Buffer Object)

**⚡ 最佳实践：手动填充 + 利用 padding 字段**

```cpp
// 推荐做法：把 padding 字段变成有用数据
struct PointLight {
    glm::vec3 position;  // offset 0,  size 12
    float radius;        // offset 12, size 4  ← padding 变成有用数据
    glm::vec3 color;     // offset 16, size 12
    float intensity;     // offset 28, size 4  ← padding 变成有用数据
};
static_assert(sizeof(PointLight) == 32, "Size check");
// 完美对齐，无浪费空间
```

**基本用法示例：相机矩阵 UBO**

```cpp
// ============== 创建 UBO ==============
#include <zk_pbr/gfx/uniform_buffer.h>

// 相机矩阵 UBO（View + Projection）
// ⚠️ 注意：必须符合 std140 对齐规则
struct CameraMatrices {
    glm::mat4 view;        // offset 0,  size 64, align 16 ✅
    glm::mat4 projection;  // offset 64, size 64, align 16 ✅
};
static_assert(sizeof(CameraMatrices) == 128, "Size check");
// mat4 天然符合 std140（列主序，每列 16 字节对齐）

UniformBuffer cameraUBO(sizeof(CameraMatrices), GL_DYNAMIC_DRAW);

// ============== 绑定到 Binding Point ==============
cameraUBO.BindToPoint(0);  // 绑定到 binding point 0

// ============== Shader 中绑定 Uniform Block ==============
shader.Use();
shader.SetUniformBlock("CameraMatrices", 0);  // 绑定到 binding point 0

// ============== 更新 UBO 数据 ==============
CameraMatrices matrices;
matrices.view = camera.GetViewMatrix();
matrices.projection = camera.GetProjectionMatrix(aspect);
cameraUBO.SetData(&matrices, sizeof(matrices));
```

## Cubemap 渲染完整示例

### Shader 代码

**skybox.vert**:
```glsl
#version 460 core
layout (location = 0) in vec3 aPos;

// 使用 UBO
layout (std140, binding = 0) uniform CameraMatrices {
    mat4 view;
    mat4 projection;
};

out vec3 TexCoords;

void main() {
    TexCoords = aPos;
    // 去除平移分量（只保留旋转）
    mat4 viewNoTranslation = mat4(mat3(view));
    vec4 pos = projection * viewNoTranslation * vec4(aPos, 1.0);
    gl_Position = pos.xyww;  // 技巧：深度值始终为 1.0
}
```

**skybox.frag**:
```glsl
#version 460 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main() {
    FragColor = texture(skybox, TexCoords);
}
```

### C++ 代码

```cpp
#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/texture_cubemap.h>
#include <zk_pbr/gfx/uniform_buffer.h>
#include <zk_pbr/gfx/camera.h>

// ============== 初始化 ==============
// 1. 创建 Shader
Shader skyboxShader("shaders/skybox.vert", "shaders/skybox.frag");

// 2. 加载 Cubemap
std::vector<std::string> faces = {
    "textures/skybox/right.jpg",
    "textures/skybox/left.jpg",
    "textures/skybox/top.jpg",
    "textures/skybox/bottom.jpg",
    "textures/skybox/front.jpg",
    "textures/skybox/back.jpg"
};
TextureCubemap cubemap(faces);

// 3. 创建天空盒立方体
Mesh skyboxCube = PrimitiveFactory::CreateCube();

// 4. 定义 UBO 数据结构（mat4 天然符合 std140 对齐）
struct CameraMatrices {
    glm::mat4 view;        // offset 0,  size 64
    glm::mat4 projection;  // offset 64, size 64
};
static_assert(sizeof(CameraMatrices) == 128, "Size check");

// 5. 创建 UBO
UniformBuffer cameraUBO(sizeof(CameraMatrices), GL_DYNAMIC_DRAW);
cameraUBO.BindToPoint(0);  // 绑定到 binding point 0

// 6. Shader 绑定 Uniform Block
skyboxShader.Use();
skyboxShader.SetUniformBlock("CameraMatrices", 0);

// ============== 渲染循环 ==============
while (!window.ShouldClose()) {
    // 更新相机矩阵到 UBO
    CameraMatrices matrices;
    matrices.view = camera.GetViewMatrix();
    matrices.projection = camera.GetProjectionMatrix(aspect);
    cameraUBO.SetData(&matrices, sizeof(matrices));
    
    // 渲染场景...
    
    // 最后渲染天空盒（深度测试改为 LEQUAL）
    glDepthFunc(GL_LEQUAL);
    skybox_shader.Use();
    skybox_shader.SetTextureCube("u_Cubemap", skybox.GetID(), 0);
    cube.Draw();
    glDepthFunc(GL_LESS);  // 恢复默认
    
    window.SwapBuffers();
}
```

## UBO 的优势

### 传统方式（每个 shader 都要设置）
```cpp
shaderA.Use();
shaderA.SetMat4("view", viewMatrix);
shaderA.SetMat4("projection", projectionMatrix);

shaderB.Use();
shaderB.SetMat4("view", viewMatrix);
shaderB.SetMat4("projection", projectionMatrix);

shaderC.Use();
shaderC.SetMat4("view", viewMatrix);
shaderC.SetMat4("projection", projectionMatrix);
```

### UBO 方式（设置一次，所有 shader 共享）
```cpp
// 只更新一次
cameraUBO.SetData(&matrices, sizeof(matrices));

// 所有 shader 自动使用相同数据
shaderA.Use();
shaderA.Draw();

shaderB.Use();
shaderB.Draw();

shaderC.Use();
shaderC.Draw();
```

**性能提升**：
- 减少 OpenGL 调用次数
- 减少 CPU-GPU 数据传输
- 大型场景中提升 10%-30% 性能

## 注意事项

### 1. std140 对齐规则（CPU 端必须遵守）

**std140 对齐规则**：
| 类型 | 大小（字节） | 对齐要求（字节） |
|------|------------|----------------|
| float, int, bool | 4 | 4 |
| vec2 | 8 | 8 |
| **vec3** | **12** | **16** ⚠️ |
| vec4 | 16 | 16 |
| mat4 | 64 | 16（每列） |
| 数组元素 | - | 16（强制） |

**关键问题：vec3 需要 16 字节对齐，但 glm::vec3 只有 12 字节！**

#### ❌ 错误示例 1：vec3 对齐问题

```cpp
// Shader 中
layout (std140) uniform Data {
    float a;      // offset 0,  size 4,  align 4
    vec3 b;       // offset 16, size 12, align 16  ← GLSL 自动对齐到 16
};

// ❌ CPU 端错误代码
struct Data {
    float a;      // offset 0,  size 4
    glm::vec3 b;  // offset 4,  size 12  ← 错位！实际应该在 offset 16
};
// sizeof(Data) = 16，但 GPU 认为 b 在 offset 16！
```

**后果**：数据错位，渲染错误或花屏。

#### ✅ 正确示例 1：手动 padding

```cpp
// 方式 1：手动填充数组
struct Data {
    float a;           // offset 0,  size 4
    float _pad[3];     // offset 4,  size 12（填充）
    glm::vec3 b;       // offset 16, size 12 ✅
};

// 方式 2：使用 alignas（推荐）
struct Data {
    float a;                 // offset 0,  size 4
    alignas(16) glm::vec3 b; // offset 16, size 12 ✅（自动填充）
};

// 方式 3：改用 vec4（最简单）
struct Data {
    float a;       // offset 0,  size 4
    glm::vec4 b;   // offset 16, size 16 ✅（xyz = 数据, w = 未使用）
};
```

#### ❌ 错误示例 2：数组对齐问题

```glsl
// Shader 中
layout (std140) uniform Lights {
    vec3 lightPositions[4];  // 每个元素对齐到 16 字节
};

// ❌ CPU 端错误代码
struct Lights {
    glm::vec3 lightPositions[4];  // 每个元素 12 字节，总共 48 字节
};
// 错误！GPU 认为每个元素占 16 字节，总共 64 字节
```

#### ✅ 正确示例 2：数组 padding

```cpp
// 方式 1：改用 vec4 数组
struct Lights {
    glm::vec4 lightPositions[4];  // 每个 16 字节 ✅
};

// 方式 2：手动 padding（不推荐，复杂）
struct Lights {
    struct PaddedVec3 {
        glm::vec3 value;
        float _pad;
    } lightPositions[4];
};
```

#### ✅ 正确示例 3：完整 Light 数据（利用 padding）

```cpp
// Shader 中
layout (std140, binding = 1) uniform LightData {
    vec3 lightColor;      // offset 0,  size 12, align 16
    float lightIntensity; // offset 12, size 4  ← padding 变成有用数据
    vec3 lightPosition;   // offset 16, size 12, align 16
    float lightRadius;    // offset 28, size 4  ← padding 变成有用数据
};

// ✅ CPU 端正确代码（推荐：手动填充 + 利用 padding）
struct LightData {
    glm::vec3 lightColor;      // offset 0,  size 12
    float lightIntensity;      // offset 12, size 4  ← vec3 后的 padding
    glm::vec3 lightPosition;   // offset 16, size 12
    float lightRadius;         // offset 28, size 4  ← vec3 后的 padding
};
static_assert(sizeof(LightData) == 32, "Size check");
// ✅ 完美对齐，无浪费空间，padding 都变成了有用数据

// 可选方式 2：改用 vec4（更简单，但语义不如上面清晰）
struct LightData {
    glm::vec4 lightColor;      // offset 0,  xyz=color, w=intensity
    glm::vec4 lightPosition;   // offset 16, xyz=pos,   w=radius
};
static_assert(sizeof(LightData) == 32, "Size check");
```

### 2. 实用工具宏（推荐添加）

```cpp
// 在项目中创建 std140_helpers.h
#pragma once
#include <glm/glm.hpp>

// std140 对齐的 vec3（自动填充到 16 字节）
struct alignas(16) std140_vec3 {
    glm::vec3 value;
    float _pad;
    
    std140_vec3() = default;
    std140_vec3(const glm::vec3& v) : value(v), _pad(0) {}
    operator glm::vec3() const { return value; }
};

// 使用示例
struct LightData {
    std140_vec3 lightColor;      // 自动 16 字节对齐
    float lightIntensity;
    std140_vec3 lightPosition;   // 自动 16 字节对齐
    int lightType;
};

// 赋值时自动转换
LightData data;
data.lightColor = glm::vec3(1.0f, 0.0f, 0.0f);
```

### 3. 验证对齐的方法

```cpp
#include <iostream>

struct CameraMatrices {
    glm::mat4 view;
    glm::mat4 projection;
};

int main() {
    std::cout << "sizeof(CameraMatrices): " << sizeof(CameraMatrices) << "\n";
    std::cout << "offsetof view: " << offsetof(CameraMatrices, view) << "\n";
    std::cout << "offsetof projection: " << offsetof(CameraMatrices, projection) << "\n";
    
    // 期望输出：
    // sizeof(CameraMatrices): 128
    // offsetof view: 0
    // offsetof projection: 64
}
```

### 3. 常见对齐错误总结

| 错误类型 | CPU 端 | GPU 端（std140） | 后果 |
|---------|--------|-----------------|------|
| vec3 无填充 | offset 4, size 12 | offset 16, size 12 | 数据错位 |
| vec3 数组 | 每个 12 字节 | 每个 16 字节 | 数组索引错误 |
| float + vec3 | offset 4 | offset 16 | 后续数据全部错位 |
| 混用基础类型 | 编译器自动对齐 | std140 强制对齐 | 不可预测 |

### 4. 建议对齐策略

**⭐ 策略 A：手动填充 + 利用 padding（最推荐）**
```cpp
struct PointLight {
    glm::vec3 position;  // offset 0,  size 12
    float radius;        // offset 12, size 4  ← padding 变成有用数据
    glm::vec3 color;     // offset 16, size 12
    float intensity;     // offset 28, size 4  ← padding 变成有用数据
};
// 优势：完美对齐，无浪费，每个字段都有实际意义
```

**策略 B：全用 vec4/mat4（最简单，推荐新手）**
```cpp
struct Data {
    glm::vec4 a;  // xyz = 数据, w = padding 或额外数据
    glm::mat4 b;  // 天然对齐
};
// 优势：不会出错，但字段语义不如方式 A 清晰
```

**策略 C：手动填充数组（仅当 padding 确实无用时）**
```cpp
struct Data {
    float a;
    float _pad0[3];  // 显式标注为 padding
    glm::vec3 b;
    float _pad1;
};
// 优势：完全显式控制，但浪费了 padding 空间
```

### ❌ Shader 中 UBO 布局不对齐示例（已废弃）

```glsl
// ❌ 错误：不对齐
layout (std140) uniform Data {
    float a;      // offset 0, size 4
    vec3 b;       // offset 4, size 12 → 错误！应该对齐到 16
};

// ✅ 正确：手动填充
layout (std140) uniform Data {
    float a;      // offset 0, size 4
    float pad[3]; // offset 4, size 12（填充）
    vec3 b;       // offset 16, size 12
};

// ✅ 或者直接用 vec4
layout (std140) uniform Data {
    vec4 a;  // xyz = 数据, w = padding
    vec3 b;
};
```

### 5. Binding Point 约定

建议统一管理 binding point：
```cpp
// binding_points.h
namespace BindingPoints {
    constexpr GLuint Camera = 0;
    constexpr GLuint Lights = 1;
    constexpr GLuint Material = 2;
}

// 使用
cameraUBO.BindToPoint(BindingPoints::Camera);
shader.SetUniformBlock("CameraMatrices", BindingPoints::Camera);
```

### 6. TextureCubemap Binding

对于 `samplerCube`，确保使用正确的纹理类型：

```cpp
// ❌ 错误：SetTexture 默认绑定 GL_TEXTURE_2D
shader.SetTexture("skybox", cubemap.GetID(), 0);  // 需要修改

// 方式 1：手动绑定（临时方案）
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.GetID());
shader.SetInt("skybox", 0);

// 方式 2：扩展 SetTexture（推荐）
shader.SetTextureCube("skybox", cubemap.GetID(), 0);
```

## 未来扩展建议

如果需要更完善的纹理 binding，可以添加：

```cpp
// shader.h
void SetTextureCube(const std::string &name, GLuint textureID, int unit) const noexcept;
void SetTexture2D(const std::string &name, GLuint textureID, int unit) const noexcept;

// shader.cpp
void Shader::SetTextureCube(const std::string &name, GLuint textureID, int unit) const noexcept {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    glUniform1i(GetUniformLocation(name), unit);
}
```

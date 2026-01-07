# Shader Binding 使用指南（Modern OpenGL 4.2+）

## 1. 纹理 Binding（推荐使用 layout(binding = N)）

### 基本用法

**Shader 中**：
```glsl
#version 460 core

// 直接指定 binding 点（推荐）
layout (binding = 0) uniform sampler2D albedoMap;
layout (binding = 1) uniform sampler2D normalMap;
layout (binding = 2) uniform samplerCube skybox;

void main() {
    vec4 albedo = texture(albedoMap, TexCoords);
    vec3 normal = texture(normalMap, TexCoords).rgb;
    vec3 env = texture(skybox, WorldPos).rgb;
}
```

**C++ 中**：
```cpp
// 使用 Shader::BindTextureToUnit
Shader::BindTextureToUnit(albedoTexture.GetID(), 0, GL_TEXTURE_2D);
Shader::BindTextureToUnit(normalTexture.GetID(), 1, GL_TEXTURE_2D);
Shader::BindTextureToUnit(skyboxTexture.GetID(), 2, GL_TEXTURE_CUBE_MAP);

// 等价于手动操作
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, albedoTexture.GetID());
```

### 关键注意事项

**⚠️ Cubemap vs 2D Texture 的区别**：
- **目标不同**：`GL_TEXTURE_CUBE_MAP` vs `GL_TEXTURE_2D`
- **采样方式不同**：Cubemap 用 vec3 方向采样，2D 用 vec2 UV
- **绑定时必须指定正确的 target**：
  ```cpp
  // ❌ 错误：Cubemap 用了 2D target
  Shader::BindTextureToUnit(cubemap.GetID(), 0, GL_TEXTURE_2D);
  
  // ✅ 正确
  Shader::BindTextureToUnit(cubemap.GetID(), 0, GL_TEXTURE_CUBE_MAP);
  ```

**⚠️ Binding 点冲突**：
- 同一个 binding 点只能绑定一个纹理
- 推荐统一管理 binding 点（用 constexpr 或 enum）
- UBO 和纹理的 binding 点是独立的（不会冲突）

---

## 2. UBO (Uniform Buffer Object)

### 基本用法

**C++ 定义数据结构**：
```cpp
#include <zk_pbr/gfx/uniform_buffer.h>

// ⚠️ 必须符合 std140 对齐规则
struct CameraMatrices {
    glm::mat4 view;        // offset 0,  size 64
    glm::mat4 projection;  // offset 64, size 64
};
static_assert(sizeof(CameraMatrices) == 128);

// 创建并绑定
UniformBuffer cameraUBO(sizeof(CameraMatrices), GL_DYNAMIC_DRAW);
cameraUBO.BindToPoint(0);  // 绑定到 binding point 0

// 更新数据
CameraMatrices matrices;
matrices.view = camera.GetViewMatrix();
matrices.projection = camera.GetProjectionMatrix(aspect);
cameraUBO.SetData(&matrices, sizeof(matrices));
```

**Shader 中使用**：
```glsl
#version 460 core

layout (std140, binding = 0) uniform CameraMatrices {
    mat4 view;
    mat4 projection;
};

void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
}
```

**C++ 绑定 Uniform Block**：
```cpp
shader.Use();
shader.SetUniformBlock("CameraMatrices", 0);  // 绑定到 binding point 0
```

### 关键注意事项

**⚠️ std140 对齐规则**：
- **vec3 必须按 16 字节对齐**（实际只用 12 字节，浪费 4 字节）
- **最佳实践**：利用 padding 字段存储有用数据
  ```cpp
  struct PointLight {
      glm::vec3 position;  // 12 bytes
      float radius;        // 4 bytes padding → 变有用数据
      glm::vec3 color;     // 12 bytes
      float intensity;     // 4 bytes padding → 变有用数据
  };  // 总共 32 bytes，完美对齐
  ```

**⚠️ mat4 对齐**：
- mat4 天然符合 std140（列主序，每列 vec4 是 16 字节）
- 无需手动 padding

**⚠️ SetUniformBlock 时机**：
- 只需调用一次（和 glUniformBlockBinding 类似）
- 通常在 shader 编译后调用一次即可

---

## 3. Cubemap 渲染示例

**skybox.vert**:
```glsl
#version 460 core
layout (location = 0) in vec3 aPos;

layout (std140, binding = 0) uniform CameraMatrices {
    mat4 view;
    mat4 projection;
};

out vec3 TexCoords;

void main() {
    TexCoords = aPos;
    mat4 viewNoTranslation = mat4(mat3(view));  // 去除平移
    vec4 pos = projection * viewNoTranslation * vec4(aPos, 1.0);
    gl_Position = pos.xyww;  // 技巧：深度值始终为 1.0
}
```

**skybox.frag**:
```glsl
#version 460 core
out vec4 FragColor;
in vec3 TexCoords;

layout (binding = 0) uniform samplerCube skybox;

void main() {
    FragColor = texture(skybox, TexCoords);
}
```

**C++ 渲染循环**:
```cpp
// 初始化（略）
UniformBuffer cameraUBO(sizeof(CameraMatrices), GL_DYNAMIC_DRAW);
cameraUBO.BindToPoint(0);

skyboxShader.Use();
skyboxShader.SetUniformBlock("CameraMatrices", 0);

// 渲染循环
while (!window.ShouldClose()) {
    // 更新 UBO
    CameraMatrices matrices{camera.GetViewMatrix(), camera.GetProjectionMatrix(aspect)};
    cameraUBO.SetData(&matrices, sizeof(matrices));
    
    // 渲染天空盒
    glDepthFunc(GL_LEQUAL);
    skyboxShader.Use();
    Shader::BindTextureToUnit(skyboxTexture.GetID(), 0, GL_TEXTURE_CUBE_MAP);
    skyboxMesh.Draw();
    glDepthFunc(GL_LESS);
}
```
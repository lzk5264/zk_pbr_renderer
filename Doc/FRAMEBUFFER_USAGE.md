# Framebuffer (FBO) 使用指南



## 1. 基本用法

### 渲染到 2D 纹理

```cpp
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/texture2d.h>

// 创建 FBO 和纹理
Framebuffer fbo(512, 512);
Texture2D colorTexture(512, 512, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);

// 附加纹理和深度缓冲
fbo.AttachTexture(colorTexture.GetID(), GL_COLOR_ATTACHMENT0);
fbo.AttachRenderbuffer(GL_DEPTH_COMPONENT24);
fbo.CheckStatus();

// 渲染到 FBO
fbo.Bind();
glViewport(0, 0, 512, 512);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
mesh.Draw();
fbo.Unbind();

// colorTexture 现在包含渲染结果
```

### 渲染到 Cubemap

```cpp
// 创建 Cubemap 和 FBO
TextureCubemap envCubemap(512, 512, GL_RGB16F);
Framebuffer captureFBO(512, 512);
captureFBO.AttachRenderbuffer(GL_DEPTH_COMPONENT24);

// 6 个面的视图矩阵（从原点看向 +X, -X, +Y, -Y, +Z, -Z）
glm::mat4 captureViews[6] = {
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
};
glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

// 渲染 6 个面
shader.Use();
shader.SetMat4("projection", captureProjection);
captureFBO.Bind();

for (int i = 0; i < 6; ++i) {
    shader.SetMat4("view", captureViews[i]);
    captureFBO.AttachCubemapFace(envCubemap.GetID(), i, GL_COLOR_ATTACHMENT0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    cubeMesh.Draw();
}

captureFBO.Unbind();
```

### 多重渲染目标 (MRT)

```cpp
// 创建 G-Buffer 纹理
Texture2D gPosition(width, height, GL_RGB16F, GL_RGB, GL_FLOAT);
Texture2D gNormal(width, height, GL_RGB16F, GL_RGB, GL_FLOAT);
Texture2D gAlbedo(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);

// 创建 FBO 并附加多个纹理
Framebuffer gBufferFBO(width, height);
gBufferFBO.AttachTexture(gPosition.GetID(), GL_COLOR_ATTACHMENT0);
gBufferFBO.AttachTexture(gNormal.GetID(), GL_COLOR_ATTACHMENT1);
gBufferFBO.AttachTexture(gAlbedo.GetID(), GL_COLOR_ATTACHMENT2);
gBufferFBO.AttachRenderbuffer(GL_DEPTH24_STENCIL8);

// 指定绘制缓冲区
GLuint attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
gBufferFBO.Bind();
glDrawBuffers(3, attachments);

// Shader 中使用 layout(location = 0/1/2) out vec4 输出到不同纹理
RenderScene();
gBufferFBO.Unbind();
```

---

## 2. IBL Irradiance Map 生成示例

```cpp
TextureCubemap GenerateIrradianceMap(const TextureCubemap& envMap, int resolution = 32) {
    // 创建目标 cubemap
    TextureCubemap irradianceMap(resolution, resolution, GL_RGB16F);
    
    // 创建 FBO
    Framebuffer captureFBO(resolution, resolution);
    captureFBO.AttachRenderbuffer(GL_DEPTH_COMPONENT24);
    
    // 渲染 6 个面
    convolutionShader.Use();
    Shader::BindTextureToUnit(envMap.GetID(), 0, GL_TEXTURE_CUBE_MAP);
    convolutionShader.SetMat4("projection", captureProjection);
    
    captureFBO.Bind();
    glViewport(0, 0, resolution, resolution);
    
    for (int i = 0; i < 6; ++i) {
        convolutionShader.SetMat4("view", captureViews[i]);
        captureFBO.AttachCubemapFace(irradianceMap.GetID(), i);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        cubeMesh.Draw();
    }
    
    captureFBO.Unbind();
    
    // 生成 mipmap
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap.GetID());
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    return irradianceMap;
}
```

---

## 3. 关键注意事项

### 窗口大小变化

**问题**：窗口 resize 后 FBO 尺寸不变
```cpp
// ❌ 错误：FBO 尺寸不变，纹理尺寸不变
void OnResize(int width, int height) {
    // FBO 仍然是旧尺寸
}
```

**解决方案**：重新创建纹理并调整 FBO
```cpp
void OnResize(int width, int height) {
    // 重新创建纹理
    colorTexture = Texture2D(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
    
    // 调整 FBO 大小（会重建内部 renderbuffer）
    fbo.Resize(width, height);
    
    // 重新附加纹理
    fbo.AttachTexture(colorTexture.GetID());
    fbo.CheckStatus();
}
```

### 深度缓冲格式选择

| 需求 | 推荐格式 | 说明 |
|------|---------|------|
| 只需要深度测试 | `GL_DEPTH_COMPONENT24` | 最常用 |
| 需要深度 + 模板 | `GL_DEPTH24_STENCIL8` | 如果用 stencil test |
| 高精度深度 | `GL_DEPTH_COMPONENT32F` | 大场景/高精度需求 |
| 需要采样深度值 | 使用深度纹理 | shader 中 `texture(depthTex, uv)` |

```cpp
// 方式 1：renderbuffer（不能在 shader 中采样）
fbo.AttachRenderbuffer(GL_DEPTH_COMPONENT24);

// 方式 2：深度纹理（可以在 shader 中采样）
Texture2D depthTexture(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
fbo.AttachTexture(depthTexture.GetID(), GL_DEPTH_ATTACHMENT);
```

**何时用 renderbuffer vs 深度纹理**：
- **renderbuffer**：只需要深度测试（性能更好）
- **深度纹理**：需要在后续 shader 中读取深度值（shadow mapping, SSAO）

### FBO 重用 vs 重新创建

```cpp
// ❌ 错误：每帧重新创建（性能差）
while (rendering) {
    Framebuffer fbo(width, height);  // 慢！
    fbo.AttachTexture(...);
    // ...
}

// ✅ 正确：创建一次，重复使用
Framebuffer fbo(width, height);
fbo.AttachTexture(...);

while (rendering) {
    fbo.Bind();
    RenderScene();
    fbo.Unbind();
}
```

### Cubemap vs 2D Texture 区别

| 特性 | Cubemap | 2D Texture |
|------|---------|-----------|
| **附加方式** | `AttachCubemapFace(id, face)` | `AttachTexture(id)` |
| **渲染次数** | 6 次（每个面） | 1 次 |
| **投影矩阵** | 90° FOV | 任意 FOV |
| **视图矩阵** | 6 个方向（±X/Y/Z） | 任意视角 |
| **用途** | IBL, Skybox, Reflection | 后处理, Shadow Map |

**Cubemap 特殊处理**：
```cpp
// Cubemap 需要渲染 6 次，每次附加不同面
for (int i = 0; i < 6; ++i) {
    captureFBO.AttachCubemapFace(cubemap.GetID(), i);  // i = 0-5 对应 +X/-X/+Y/-Y/+Z/-Z
    shader.SetMat4("view", captureViews[i]);           // 6 个不同的视图矩阵
    RenderCube();
}
```
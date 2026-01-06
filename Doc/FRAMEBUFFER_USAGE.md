# Framebuffer (FBO) 使用指南

## 基本概念

Framebuffer Object (FBO) 用于离屏渲染，将渲染结果输出到纹理而不是屏幕。

**常见用途**：
- IBL 预处理（Equirectangular → Cubemap，卷积生成 irradiance map）
- 阴影贴图（Shadow Mapping）
- 后处理效果（Bloom, SSAO, Tone Mapping）
- 镜面反射/折射
- 延迟渲染（G-Buffer）

---

## 基本用法

### 1. 渲染到 2D 纹理

```cpp
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/texture2d.h>

// 创建 FBO
Framebuffer fbo(512, 512);

// 创建颜色纹理
Texture2D colorTexture(512, 512, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);

// 附加纹理
fbo.AttachTexture(colorTexture.GetID(), GL_COLOR_ATTACHMENT0);

// 附加深度缓冲（可选）
fbo.AttachRenderbuffer(GL_DEPTH_COMPONENT24);

// 检查完整性
fbo.CheckStatus();

// 渲染到 FBO
fbo.Bind();
glViewport(0, 0, 512, 512);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

shader.Use();
mesh.Draw();

fbo.Unbind();

// 现在 colorTexture 包含了渲染结果
```

---

### 2. 渲染到 Cubemap（IBL 预处理）

```cpp
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/texture_cubemap.h>

// 创建 Cubemap
TextureCubemap envCubemap(512, 512, GL_RGB16F);

// 创建 FBO
Framebuffer captureFBO(512, 512);
captureFBO.AttachRenderbuffer(GL_DEPTH_COMPONENT24);

// 6 个面的视图矩阵
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
captureFBO.Bind();
shader.Use();
shader.SetMat4("projection", captureProjection);

for (int i = 0; i < 6; ++i) {
    shader.SetMat4("view", captureViews[i]);
    
    // 附加 cubemap 的某一面到 FBO
    captureFBO.AttachCubemapFace(envCubemap.GetID(), i, GL_COLOR_ATTACHMENT0);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    cubeMesh.Draw();
}

captureFBO.Unbind();

// 现在 envCubemap 包含了 6 个面的渲染结果
```

---

### 3. 多重渲染目标 (MRT)

```cpp
// 创建多个颜色纹理（用于延迟渲染的 G-Buffer）
Texture2D gPosition(width, height, GL_RGB16F, GL_RGB, GL_FLOAT);
Texture2D gNormal(width, height, GL_RGB16F, GL_RGB, GL_FLOAT);
Texture2D gAlbedo(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);

// 创建 FBO
Framebuffer gBufferFBO(width, height);
gBufferFBO.AttachTexture(gPosition.GetID(), GL_COLOR_ATTACHMENT0);
gBufferFBO.AttachTexture(gNormal.GetID(), GL_COLOR_ATTACHMENT1);
gBufferFBO.AttachTexture(gAlbedo.GetID(), GL_COLOR_ATTACHMENT2);
gBufferFBO.AttachRenderbuffer(GL_DEPTH24_STENCIL8);

// 指定绘制缓冲区
GLuint attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
gBufferFBO.Bind();
glDrawBuffers(3, attachments);

// 渲染几何体到 G-Buffer
shader.Use();
RenderScene();

gBufferFBO.Unbind();

// Shader 中使用 layout(location = 0/1/2) out vec4 输出到不同纹理
```

---

## 完整示例：IBL Irradiance Map 生成

```cpp
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/texture_cubemap.h>
#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/mesh.h>

class IBLPreprocessor {
public:
    IBLPreprocessor() 
        : convolutionShader_("shaders/cubemap.vert", "shaders/irradiance_convolution.frag"),
          cubeMesh_(PrimitiveFactory::CreateCube())
    {
        SetupCaptureMatrices();
    }
    
    TextureCubemap GenerateIrradianceMap(const TextureCubemap& envMap, int resolution = 32) {
        // 创建目标 cubemap
        TextureCubemap irradianceMap(resolution, resolution, GL_RGB16F);
        
        // 创建 FBO
        Framebuffer captureFBO(resolution, resolution);
        captureFBO.AttachRenderbuffer(GL_DEPTH_COMPONENT24);
        captureFBO.CheckStatus();
        
        // 渲染 6 个面
        convolutionShader_.Use();
        convolutionShader_.SetTextureCube("environmentMap", envMap.GetID(), 0);
        convolutionShader_.SetMat4("projection", captureProjection_);
        
        captureFBO.Bind();
        glViewport(0, 0, resolution, resolution);
        
        for (int i = 0; i < 6; ++i) {
            convolutionShader_.SetMat4("view", captureViews_[i]);
            captureFBO.AttachCubemapFace(irradianceMap.GetID(), i);
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            cubeMesh_.Draw();
        }
        
        captureFBO.Unbind();
        
        // 生成 mipmap（可选）
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap.GetID());
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        
        return irradianceMap;
    }
    
private:
    Shader convolutionShader_;
    Mesh cubeMesh_;
    glm::mat4 captureProjection_;
    glm::mat4 captureViews_[6];
    
    void SetupCaptureMatrices() {
        captureProjection_ = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        
        captureViews_[0] = glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        captureViews_[1] = glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        captureViews_[2] = glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f));
        captureViews_[3] = glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f));
        captureViews_[4] = glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        captureViews_[5] = glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
    }
};
```

---

## 注意事项

### 1. 窗口大小变化

```cpp
void OnResize(int width, int height) {
    // 需要重新创建纹理
    colorTexture = Texture2D(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
    
    // 调整 FBO 大小（会重建 renderbuffer）
    fbo.Resize(width, height);
    
    // 重新附加纹理
    fbo.AttachTexture(colorTexture.GetID());
    fbo.CheckStatus();
}
```

### 2. 深度缓冲选择

| 需求 | 推荐格式 |
|------|---------|
| 只需要深度测试 | GL_DEPTH_COMPONENT24 |
| 需要深度 + 模板 | GL_DEPTH24_STENCIL8 |
| 高精度深度 | GL_DEPTH_COMPONENT32F |
| 需要读取深度值 | 使用深度纹理而不是 renderbuffer |

```cpp
// 使用 renderbuffer（不能采样）
fbo.AttachRenderbuffer(GL_DEPTH_COMPONENT24);

// 使用深度纹理（可以在 shader 中采样）
Texture2D depthTexture(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
fbo.AttachDepthTexture(depthTexture.GetID());
```

### 3. 性能优化

```cpp
// ❌ 错误：每帧重新创建 FBO
while (rendering) {
    Framebuffer fbo(width, height);  // 慢！
    // ...
}

// ✅ 正确：创建一次，重复使用
Framebuffer fbo(width, height);
while (rendering) {
    fbo.Bind();
    // ...
    fbo.Unbind();
}
```

### 4. 恢复默认 Framebuffer

```cpp
// 渲染到 FBO
fbo.Bind();
RenderScene();

// 恢复默认 framebuffer（屏幕）
fbo.Unbind();  // 等价于 glBindFramebuffer(GL_FRAMEBUFFER, 0)

// 渲染 UI 到屏幕
RenderUI();
```

---

## 对比：UBO vs FBO

| 特性 | UniformBuffer (UBO) | Framebuffer (FBO) |
|------|---------------------|-------------------|
| **用途** | 存储 uniform 数据 | 渲染到纹理 |
| **API** | glGenBuffers | glGenFramebuffers |
| **绑定点** | Binding point (0-N) | GL_FRAMEBUFFER |
| **数据更新** | glBufferSubData | 渲染操作写入 |
| **生命周期** | 长期存在，频繁更新 | 按需创建，用完可销毁 |
| **典型场景** | 相机矩阵、光照参数 | 后处理、阴影贴图、IBL |

**架构设计**：
- UBO 和 FBO 是**独立的类**，职责不同
- UBO：数据管理（Buffer）
- FBO：渲染目标（Framebuffer）

---

## 工业界标准对比

| 引擎 | UBO 类 | FBO 类 | 架构 |
|------|--------|--------|------|
| **Unity** | ComputeBuffer | RenderTexture | 独立类 |
| **Unreal** | FUniformBuffer | FRHIFramebuffer | 独立类 |
| **Godot** | UniformBuffer | FramebufferCache | 独立类 |
| **你的项目** | UniformBuffer | Framebuffer | 独立类 ✅ |

所有主流引擎都将 UBO 和 FBO 作为独立类实现。

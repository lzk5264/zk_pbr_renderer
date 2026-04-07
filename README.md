# ZK PBR Renderer

一个使用 C++17 / OpenGL 4.6 从零编写的 PBR（Physically Based Rendering）渲染器。项目核心是手写完整的 IBL（Image-Based Lighting）预计算管线，基于 Split-Sum Approximation 实现漫反射与镜面反射两项环境光照，支持加载 glTF 2.0 模型与 HDR 环境贴图。

## 渲染截图

<!-- 在此处放置渲染截图，格式示例：-->
<!-- ![场景描述](screenshots/xxx.png) -->

## 功能概述

- **PBR 着色** — Cook-Torrance 微表面 BRDF，Metallic-Roughness 工作流，支持 Albedo / Normal / MetallicRoughness / AO / Emissive 五通道贴图
- **IBL 环境光照** — 从一张 HDR 等距矩形贴图出发，在 GPU 上预计算 Irradiance Map、Prefiltered Environment Map 和 DFG LUT 三张纹理，运行时通过 Split-Sum 查表完成环境光着色
- **HDR 后处理** — 场景渲染到 RGBA16F 浮点 FBO，经 ACES Filmic Tone Mapping + Gamma 校正输出到屏幕
- **glTF 2.0 模型导入** — 使用 cgltf 递归遍历场景图，提取顶点数据与 PBR 材质，缺失属性与贴图自动填充默认值
- **Skybox** — 将 HDR 环境贴图转为 Cubemap，作为天空盒渲染

## 技术栈

| 类别 | 技术 |
|------|------|
| 图形 API | OpenGL 4.6 Core Profile |
| 语言标准 | C++17 |
| 窗口 / 上下文 | GLFW 3 |
| GL 加载 | GLAD |
| 数学库 | GLM |
| 模型加载 | cgltf (glTF 2.0) |
| 图像加载 | stb_image (.hdr / LDR) · tinyexr (.exr) |
| 构建系统 | CMake 3.20+ |

---

## 实现细节

### IBL 预计算管线

IBL 是本项目的核心模块。启动时，`SceneEnvironment::LoadFromHDR()` 按以下顺序在 GPU 上完成所有预计算，无需任何离线烘焙工具：

```
HDR 等距矩形贴图 (.exr / .hdr)
        │
        ▼
Equirectangular → Cubemap 转换
  将 HDR 数据上传为 2D 纹理，通过 6 次 FBO 渲染（90° FOV 投影 + 球面坐标映射）
  生成 1024² × 6 面 RGB16F Cubemap
        │
        ├──────────────────┬──────────────────┐
        ▼                  ▼                  ▼
 Irradiance Map    Prefiltered Env Map    DFG LUT
 32²/面 RGB16F     256²/面 RGB16F         512² RG16F
 Cosine-weighted   GGX 重要性采样         仅依赖 BRDF
 半球采样           9 级 mipmap            与环境贴图无关
        │                  │                  │
        └──────────────────┴──────────────────┘
                           │
                           ▼
              片段着色器 evaluateIBL()
          漫反射: Fresnel 加权采样 Irradiance Map
          镜面反射: Split-Sum = Prefiltered LD × DFG 查表
```

三张预计算贴图的实现要点：

| 预计算纹理 | 采样策略 | 关键细节 |
|-----------|----------|---------|
| **Irradiance Map** | Hammersley 序列 + Cosine-weighted 半球采样 | PDF = cos(θ)/π 与 Lambert 被积函数匹配，MC 方差最小；32² 分辨率即可，漫反射为低频信号 |
| **Prefiltered Env Map** | Hammersley 序列 + GGX NDF 重要性采样 | 不同粗糙度存入不同 mipmap 层级；基于 Ωs/Ωp 比值动态选择采样 mip 以抑制高粗糙度噪声 |
| **DFG LUT** | 固定 N=(0,0,1)，从 NdotV 反推 V | Height-Correlated Smith GGX 可见性函数；支持单散射/多散射两种模式 (`is_multiscatter` 切换) |

预计算使用的共享工具：
- `RenderToCubemapFaces()` — 复用的 FBO 渲染函数，6 个方向的 90° 视图矩阵 + 临时 UBO (binding=15)
- Hammersley 序列 — 基于 Van der Corput 位翻转的准随机序列
- `GetONB()` — Frisvad 改进的正交基构建（处理极点奇异性），将切线空间采样方向旋转到世界空间

### glTF 2.0 模型导入

`Model::LoadFromGLTF()` 通过 cgltf 加载 glTF 文件，递归遍历场景图节点树，逐节点累积变换矩阵，提取每个 primitive 的顶点数据与材质参数。

- **顶点格式**：固定 12 floats/vertex — Position(3) + Normal(3) + UV(2) + Tangent(4)，缺失属性填充默认值（如 N=(0,1,0)、T=(1,0,0,1)）
- **材质提取**：读取 base_color / metallic / roughness / emissive factor 及 5 张 PBR 贴图；颜色贴图 (albedo, emissive) 使用 `GL_SRGB8_ALPHA8`，数据贴图 (normal, MR, AO) 使用 `GL_RGBA8`
- **缺失贴图兜底**：`DefaultTextures` 提供 1×1 纯色占位纹理（White / FlatNormal / Black / DefaultMR），保证 shader 采样始终合法
- **资源管理**：`cgltf_data` 通过 `unique_ptr` + custom deleter 实现 RAII

### 渲染循环

每帧包含三个 pass：

1. **PBR Pass** — 绑定 HDR FBO (RGBA16F)，上传 CameraUBO，对每个 DrawCommand 绑定材质贴图 (slot 0-4) + IBL 贴图 (slot 5-7) + ObjectUBO，执行绘制
2. **Skybox Pass** — `glDepthFunc(GL_LEQUAL)` 令天空盒深度恒为 1.0，绑定环境 Cubemap 绘制单位立方体
3. **Post-Process Pass** — 全屏三角形（`gl_VertexID` 生成，无需 VBO），采样 HDR color attachment 执行 ACES Tone Mapping + Gamma 校正

### 工程设计

- **统一绑定协议**：所有 texture slot、UBO binding point、material uniform location 集中定义在 `render_constants.h`，shader 与 C++ 侧引用同一套常量
- **GPU 资源 RAII**：Texture2D / TextureCubemap / Mesh / Framebuffer / UniformBuffer 均为 move-only RAII 类，析构时自动释放 GL 对象

### 资源绑定表

**Texture Slots**

| Slot | 资源 | 绑定方 |
|------|------|--------|
| 0-4 | Albedo / Normal / MetallicRoughness / AO / Emissive | `Material::Bind()` |
| 5 | Irradiance Map | `SceneEnvironment::BindIBL()` |
| 6 | Prefiltered Env Map | `SceneEnvironment::BindIBL()` |
| 7 | DFG LUT | `SceneEnvironment::BindIBL()` |

**UBO Binding Points**

| Binding | 数据 | 更新频率 |
|---------|------|----------|
| 0 | CameraUBO (view, projection, cameraPos) | 每帧 |
| 1 | ObjectUBO (model, modelInvTranspose) | 每 draw call |

---

## 项目结构

```
zk_pbr_renderer/
├── include/zk_pbr/
│   ├── core/
│   │   └── window.h                  # GLFW 窗口封装
│   └── gfx/
│       ├── render_constants.h        # 统一绑定协议 (texture slot, UBO binding, uniform location)
│       ├── texture2d.h               # 2D 纹理 RAII + DFG LUT 计算
│       ├── texture_cubemap.h         # Cubemap RAII + IBL 预计算 (Irradiance / Prefiltered / Equirect→Cube)
│       ├── scene_environment.h       # IBL 资源聚合与绑定
│       ├── material.h                # PBR 材质 (5 贴图 + factors + DefaultTextures)
│       ├── model.h                   # glTF 场景图遍历、网格与材质提取
│       ├── mesh.h                    # VAO/VBO/EBO RAII + 内置基本体
│       ├── shader.h                  # Shader 编译/链接
│       ├── framebuffer.h             # FBO RAII
│       ├── uniform_buffer.h          # UBO RAII
│       ├── camera.h                  # 透视相机
│       └── camera_controller.h       # 编辑器风格相机控制器
├── src/                              # 对应头文件的实现
├── shaders/
│   ├── IBL/                          # PBR 着色 + IBL 预计算着色器
│   └── common/                       # Skybox / 后处理 / 工具着色器
├── resources/
│   ├── models/                       # glTF 模型资源
│   └── textures/                     # HDR 环境贴图
├── Doc/                              # 模块设计笔记
└── CMakeLists.txt
```

## 构建与运行

**环境要求**：Windows · Visual Studio 2022 · 支持 OpenGL 4.6 的 GPU

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

CMake 会自动将 `resources/` 和 `shaders/` 复制到可执行文件目录。

**操控方式**

| 操作 | 效果 |
|------|------|
| 右键 + WASD / QE | 飞行移动 |
| 中键拖拽 | 平移视图 |
| 滚轮 | 缩放（调整 FOV） |
| ESC | 退出 |

# ZK PBR Renderer

基于 OpenGL 4.6 的 PBR (Physically Based Rendering) 渲染器，实现了完整的 **Image-Based Lighting (IBL)** 管线，支持 glTF 2.0 模型导入与 HDR 环境光照。

## 渲染效果

- **PBR 材质渲染** - Cook-Torrance BRDF，Metallic-Roughness 工作流
- **IBL 环境光照** - Split-Sum Approximation，包含漫反射与镜面反射两项
- **HDR 渲染管线** - 浮点 FBO → Tone Mapping (ACES Filmic) → Gamma 校正
- **glTF 2.0 模型导入** - 场景图遍历、PBR 材质贴图、法线贴图
- **Skybox 渲染** - 基于环境 Cubemap 的天空盒

## 技术栈

| 类别 | 技术 |
|------|------|
| 图形 API | OpenGL 4.6 Core Profile |
| 窗口/上下文 | GLFW 3 |
| GL 加载 | GLAD |
| 数学库 | GLM |
| 模型加载 | cgltf (glTF 2.0) |
| LDR 图像 | stb_image |
| HDR 图像 | stb_image (.hdr) / tinyexr (.exr) |
| 构建系统 | CMake 3.20+ |
| 语言标准 | C++17 |

## 项目结构

```
zk_pbr_renderer/
├── include/zk_pbr/
│   ├── core/
│   │   └── window.h                  # GLFW 窗口封装
│   └── gfx/
│       ├── render_constants.h        # 全局绑定协议 (texture slot, UBO binding, uniform location)
│       ├── texture_parameters.h      # TextureSpec / TexturePresets / 纹理枚举
│       ├── texture2d.h               # 2D 纹理 RAII + DFG LUT 计算
│       ├── texture_cubemap.h         # Cubemap RAII + IBL 三大预计算
│       ├── scene_environment.h       # IBL 资源聚合 (IBLConfig + BindIBL)
│       ├── image_loader.h            # HDR/LDR 图像加载
│       ├── shader.h                  # Shader 编译/链接
│       ├── mesh.h                    # VAO/VBO/EBO RAII + PrimitiveFactory
│       ├── vertex_layout.h           # 顶点属性布局描述
│       ├── material.h                # PBR 材质 (5 贴图 + factors + DefaultTextures 兜底)
│       ├── model.h                   # glTF 模型 (MeshPrimitive + DrawCommand)
│       ├── framebuffer.h             # FBO RAII
│       ├── uniform_buffer.h          # UBO RAII
│       ├── camera.h                  # 透视相机
│       └── camera_controller.h       # 编辑器风格相机控制器
├── src/
│   ├── main.cpp                      # 渲染循环入口
│   ├── core/window.cpp
│   └── gfx/                          # 各模块实现
├── shaders/
│   ├── IBL/                          # PBR 着色 + IBL 预计算着色器
│   └── common/                       # Skybox / 后处理 / 工具着色器
├── resources/
│   ├── models/                       # glTF 模型资源
│   └── textures/                     # HDR 环境贴图
├── Doc/                              # 模块设计文档
└── CMakeLists.txt
```

---

## 核心管线 1: IBL (Image-Based Lighting)

### 概述

IBL 使用一张 HDR 环境贴图作为全局光源，预计算漫反射和镜面反射两个积分项。核心入口为 `SceneEnvironment::LoadFromHDR()`，内部按依赖顺序生成四张预计算贴图。

### 流程图

```
                    HDR 等距矩形贴图 (.exr / .hdr)
                                 |
                                 v
                +--------------------------------+
                |  LoadHDRImage()                 |
                |  stb_image (.hdr) 或            |
                |  tinyexr (.exr)                 |
                |  输出: float* 像素数据          |
                +---------------+----------------+
                                |
                                v
                +--------------------------------+
                |  Equirectangular -> Cubemap     |
                |  TextureCubemap::               |
                |    LoadFromEquirectangular()    |
                |                                |
                |  方法:                          |
                |  1. 将 HDR 数据上传为 2D 纹理  |
                |  2. 创建 1024^3 RGB16F Cubemap  |
                |  3. 对 6 个面各渲染一次:        |
                |     - 用 90 度 FOV 投影矩阵     |
                |     - 片段着色器将 localPos     |
                |       转为球面坐标 (phi, theta) |
                |       再映射到等距矩形 UV       |
                |  4. glGenerateMipmap            |
                +---------------+----------------+
                                |
                 +--------------+---------------+
                 |              |               |
                 v              v               v
        +---------------+ +---------------+ +-----------------+
        |  Irradiance   | |  Prefiltered  | |  DFG LUT        |
        |  Map          | |  Env Map      | |                 |
        |               | |               | |  Texture2D::    |
        |  Cubemap::    | |  Cubemap::    | |  ComputeDFG()   |
        |  Convolve     | |  Prefiltered  | |                 |
        |  Irradiance() | |  EnvMap()     | |  仅依赖 BRDF   |
        |               | |               | |  与 Cubemap 无关|
        +-------+-------+ +-------+-------+ +--------+--------+
                |                |                    |
                v                v                    v
        +---------------+ +---------------+ +-----------------+
        | 32^3 RGB16F   | | 256^3 RGB16F  | | 512^2 RG16F     |
        | 无 mipmap     | | 9 级 mipmap   | | x: NdotV        |
        | slot 5        | | slot 6        | | y: roughness    |
        +-------+-------+ +-------+-------+ | slot 7          |
                |                |           +--------+--------+
                |                |                    |
                v                v                    v
        +--------------------------------------------------+
        |  SceneEnvironment::BindIBL()                      |
        |  绑定到 texture slot 5, 6, 7                      |
        +-------------------------+------------------------+
                                  |
                                  v
        +--------------------------------------------------+
        |  pbr_shading_fs.frag :: evaluateIBL()             |
        |                                                   |
        |  漫反射:                                          |
        |    F = fresnelSchlick(NdotV, f0)                  |
        |    irr = texture(IrradianceMap, N).rgb             |
        |    diffuse = (1-F) * diffuseColor / pi * irr      |
        |                                                   |
        |  镜面反射 (Split-Sum):                            |
        |    R = reflect(-V, N)                             |
        |    LD = textureLod(PrefilteredMap, R,              |
        |                   roughness * maxMip)             |
        |    DFG = texture(DFGLUT, (NdotV, roughness)).rg   |
        |    spec = (f0*DFG.x + f90*DFG.y) * LD            |
        |                                                   |
        |  color = (diffuse + specular) * AO + emissive     |
        +--------------------------------------------------+
```

### 三张预计算贴图详解

#### 1. Irradiance Map (漫反射辐照度图)

| 项目 | 说明 |
|------|------|
| **目的** | 预计算半球所有方向入射光对 Lambert 漫反射的贡献积分 |
| **数学** | `L_irr(N) = integrate[ L_i(w) * cos(theta) dw ]` over hemisphere |
| **采样** | Hammersley 低差异序列 + Cosine-weighted 半球采样 (Concentric Disk Mapping) |
| **为什么 Cosine-weighted** | PDF = cos(theta)/pi 与被积函数匹配，MC 估计简化为 `(pi/N) * sum(L_i)`，方差最小 |
| **分辨率** | 32x32/face，漫反射是极低频信号，更高分辨率收益可忽略 |
| **潜在优化** | 可用 L2 球谐函数 (SH) 替代 Cubemap，仅需 9 个 RGB 系数 |

#### 2. Prefiltered Environment Map (预滤波环境贴图)

| 项目 | 说明 |
|------|------|
| **目的** | 针对不同粗糙度预计算环境贴图的模糊版本 (Split-Sum 的 LD 项) |
| **数学** | `LD(R,a) = sum[L_i(w)*NdotL] / sum[NdotL]`，其中 R=N=V (各向同性假设) |
| **采样** | Hammersley + GGX NDF 重要性采样，不同粗糙度存储在不同 mipmap 层级 |
| **Mip 级别选择** | 基于 Omega_s/Omega_p 比值动态选择采样 mip，避免高粗糙度下欠采样噪声 |
| **分辨率** | 256x256 (mip 0)，9 级 mipmap (256->1) |
| **潜在优化** | 多散射能量补偿 (Multi-scattering)，当前单散射近似在高粗糙度金属面偏暗 |

#### 3. DFG LUT (BRDF 积分查找表)

| 项目 | 说明 |
|------|------|
| **目的** | 预计算 Split-Sum 的环境 BRDF 积分项 |
| **数学** | `integrate[f_r * cos(theta) dw] ≈ F0 * DFG1 + F90 * DFG2` |
| **参数化** | x = NdotV, y = perceptual roughness, 输出 RG 双通道 |
| **关键实现** | 固定 N=(0,0,1) 从 NdotV 反推 V；Height-Correlated Smith GGX 可见性函数 |
| **多散射版本** | `is_multiscatter` 参数切换单散射/多散射 LUT 计算 |
| **分辨率** | 512x512, RG16F |
| **潜在优化** | 可用解析拟合 (如 Karis 多项式) 替代 LUT，省一次纹理采样 |

### 关键共享工具

- **RenderToCubemapFaces()** - 内部复用的 FBO 渲染函数，设置 6 个方向的 90 度视图矩阵 + 临时 UBO (binding=15)，对每个面执行一次绘制
- **Hammersley 序列** - 基于 Van der Corput 位翻转的准随机序列，各预计算 shader 共用
- **GetONB()** - Frisvad 改进的正交基构建（无分支 singularity），用于将切线空间采样方向旋转到世界空间

---

## 核心管线 2: glTF 2.0 模型导入

### 概述

使用 cgltf 库解析 glTF 2.0 文件，递归遍历场景图，提取网格顶点数据与 PBR 材质。核心入口为 `Model::LoadFromGLTF()`。

### 流程图

```
                     .gltf 文件
                          |
          +---------------+----------------+
          |               |                |
          v               v                v
  cgltf_parse_file  cgltf_load_buffers  cgltf_validate
  (解析 JSON)       (加载 bin 数据)     (完整性校验)
                          |
                          v
              遍历 scene->nodes[]
              对每个根节点调用 TraverseNode()
                          |
                          v
        +-------------------------------------+
        |  TraverseNode() 递归                 |
        |                                     |
        |  1. cgltf_node_transform_local()    |
        |     获取节点局部变换矩阵            |
        |  2. world_matrix = parent * local   |
        |     累积变换链                       |
        |  3. if node->mesh:                  |
        |     对每个 primitive:               |
        |       ExtractMesh() + ExtractMaterial()|
        |     -> 存入 MeshPrimitive           |
        |  4. 递归 children                   |
        +--------+-----------------+----------+
                 |                 |
                 v                 v
        +----------------+  +---------------------+
        | ExtractMesh()  |  | ExtractMaterial()   |
        |                |  |                     |
        | 顶点属性提取:  |  | PBR 参数提取:       |
        |  POSITION(vec3)|  |  base_color_factor  |
        |  NORMAL  (vec3)|  |  metallic_factor    |
        |  TEXCOORD(vec2)|  |  roughness_factor   |
        |  TANGENT (vec4)|  |  emissive_factor    |
        |                |  |                     |
        | 缺失属性默认值:|  | 5 张材质贴图:       |
        |  N=(0,1,0)     |  |  albedo     (sRGB)  |
        |  UV=(0,0)      |  |  normal     (linear)|
        |  T=(1,0,0,1)   |  |  metal_rough(linear)|
        |                |  |  ao         (linear)|
        | 12 floats/vert |  |  emissive   (sRGB)  |
        | -> Mesh (GPU)  |  |                     |
        +----------------+  | 空贴图 -> nullptr   |
                             | -> DefaultTextures  |
                             |    兜底 (1x1 纯色)  |
                             +---------------------+
```

### 关键设计决策

- **顶点格式固定 12 floats**: pos(3) + normal(3) + uv(2) + tangent(4)，对应 `layouts::PBRVertex()`
- **材质兜底机制**: `DefaultTextures` 提供 1x1 纯色纹理 (White/FlatNormal/Black/DefaultMR)，确保 shader 采样合法
- **sRGB 区分**: 颜色贴图 (albedo, emissive) 用 `GL_SRGB8_ALPHA8`，数据贴图 (normal, MR, AO) 用 `GL_RGBA8`
- **Sampler 解析**: glTF sampler 枚举值与 GL 枚举一致，直接 `static_cast` 转换
- **URI 解码**: 处理路径中的 `%20` 等百分号编码
- **RAII 资源管理**: `cgltf_data` 用 `unique_ptr + custom deleter` 保证异常安全释放

---

## 渲染循环

```
每帧:
  1. 更新相机 (EditorCameraController::Update)
  2. 绑定 HDR FBO (RGBA16F)
  3. 清屏
  4. 上传 CameraUBO (view + projection + cameraPos)
  5. PBR Pass:
     ├── 激活 pbr_shader
     ├── SceneEnvironment::BindIBL() -> slot 5,6,7
     └── 遍历 DrawCommand:
         ├── Material::Bind() -> slot 0-4 + uniform factors
         ├── 上传 ObjectUBO (model + model_inv_t)
         └── Mesh::Draw()
  6. Skybox Pass:
     ├── glDepthFunc(GL_LEQUAL) (天空盒深度 = 1.0)
     ├── 激活 skybox_shader
     ├── 绑定 env_cubemap -> slot 0
     ├── 绘制单位立方体
     └── glDepthFunc(GL_LESS)
  7. 解绑 HDR FBO
  8. Post-Process Pass:
     ├── 禁用深度测试
     ├── 激活 postprocess_shader (ACES Tone Mapping + Gamma)
     ├── 绑定 HDR color texture -> slot 0
     ├── 全屏三角形绘制 (gl_VertexID, 无 VBO)
     └── 恢复深度测试
  9. SwapBuffers + PollEvents
```

## 资源绑定协议

所有 shader 共享统一的绑定约定，定义在 `render_constants.h`:

### Texture Slots

| Slot | 资源 | 绑定方 | Shader 采样器 |
|------|------|--------|---------------|
| 0 | Albedo | Material::Bind() | `u_AlbedoTex` |
| 1 | Normal | Material::Bind() | `u_NormalTex` |
| 2 | Metallic/Roughness | Material::Bind() | `u_MetallicRoughnessTex` |
| 3 | AO | Material::Bind() | `u_AOTex` |
| 4 | Emissive | Material::Bind() | `u_EmissiveTex` |
| 5 | Irradiance Map | SceneEnvironment::BindIBL() | `u_IrradianceMap` |
| 6 | Prefiltered Env Map | SceneEnvironment::BindIBL() | `u_PrefilteredEnvMap` |
| 7 | DFG LUT | SceneEnvironment::BindIBL() | `u_DFGLUT` |

### UBO Binding Points

| Binding | 数据 | 大小 | 更新频率 |
|---------|------|------|----------|
| 0 | CameraUBO (view, proj, cameraPos) | 144 bytes | 每帧 |
| 1 | ObjectUBO (model, model_inv_t) | 128 bytes | 每 draw call |
| 15 | 内部临时 (Cubemap 渲染用) | 144 bytes | 仅预计算阶段 |

### Material Uniform Locations (explicit layout)

| Location | Uniform | 类型 |
|----------|---------|------|
| 0 | u_BaseColor | vec4 |
| 1 | u_MetallicFactor | float |
| 2 | u_RoughnessFactor | float |
| 3 | u_EmissiveFactor | vec3 |

---

## 构建

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

运行前确保 `resources/` 和 `shaders/` 目录位于可执行文件同级目录（CMake 会自动复制）。

### 控制

- **右键 + WASD/QE** - 移动相机
- **中键拖拽** - 平移
- **滚轮** - 缩放 (FOV)
- **ESC** - 退出

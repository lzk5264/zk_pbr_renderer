# Texture 系统设计文档


### 核心组件

```
texture_parameters.h  - 纹理参数定义（enum class + 预设）
texture.h             - TextureHandle（RAII）+ TextureLoader（stb_image）
texture2d.h/cpp       - Texture2D 类
texture_cubemap.h/cpp - TextureCubemap 类
```



## 使用示例

### Texture2D

#### 方式 1：从文件加载
```cpp
auto diffuse = Texture2D::LoadFromFile("diffuse.png", texture_presets::Diffuse());
auto normal = Texture2D::LoadFromFile("normal.png", texture_presets::Normal());

// HDR 纹理（天空盒）
auto skybox = Texture2D::LoadFromFile("sky.exr", texture_presets::HDRSkybox());

// HDR 纹理（IBL 环境光）
auto env_map = Texture2D::LoadFromFile("env.hdr", texture_presets::HDRIBL());
```

#### 方式 2：从内存创建
```cpp
TextureSpecification spec;
spec.internal_format = TextureInternalFormat::RGBA8;
spec.min_filter = TextureFilter::Linear;

auto tex = Texture2D(width, height, pixel_data, spec);
```

#### 方式 3：创建空纹理（用于 FBO）
```cpp
TextureSpecification spec;
spec.internal_format = TextureInternalFormat::RGB16F;
spec.generate_mipmaps = false;

auto color_attachment = Texture2D(1920, 1080, spec);
```

#### 使用纹理
```cpp
// 绑定到纹理单元
diffuse.Bind(0);     // GL_TEXTURE0
normal.Bind(1);      // GL_TEXTURE1

// 在着色器中
shader.SetInt("diffuseMap", 0);
shader.SetInt("normalMap", 1);

// 更新部分数据
diffuse.SetData(new_pixels, 512, 512, x_offset, y_offset);

// 重新生成 mipmap
diffuse.GenerateMipmaps();

// 动态修改参数
diffuse.SetWrapMode(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
diffuse.SetFilterMode(TextureFilter::LinearMipmapLinear, TextureFilter::Linear);
diffuse.SetAnisotropy(16.0f);
```

### TextureCubemap

#### 方式 1：从 6 张图片加载
```cpp
std::array<std::string, 6> faces = {
    "skybox/right.jpg",   // +X
    "skybox/left.jpg",    // -X
    "skybox/top.jpg",     // +Y
    "skybox/bottom.jpg",  // -Y
    "skybox/front.jpg",   // +Z
    "skybox/back.jpg"     // -Z
};

auto skybox = TextureCubemap::LoadFromFiles(faces);
```

#### 方式 2：创建空 cubemap
```cpp
// IBL 用途（需要 mipmap）
TextureSpecification spec = texture_presets::HDRIBL();
auto irradiance_map = TextureCubemap(32, spec); // 32x32 cubemap
```

#### 使用 cubemap
```cpp
skybox.Bind(0);
shader.SetInt("skybox", 0);

// 更新单个面
skybox.SetFaceData(TextureCubemap::FACE_POSITIVE_X, data, size, size);

// 设置参数（cubemap 通常用 ClampToEdge）
skybox.SetWrapMode(TextureWrap::ClampToEdge, 
                   TextureWrap::ClampToEdge, 
                   TextureWrap::ClampToEdge);
```

## 高级特性

### 1. sRGB 色彩空间支持
```cpp
TextureSpecification spec;
spec.srgb = true;  // 自动使用 SRGB8_ALPHA8
auto tex = Texture2D::LoadFromFile("color.png", spec);
```

### 2. HDR 纹理
```cpp加载

#### 支持的格式
- `.hdr` - Radiance RGBE 格式（通过 stb_image）
- `.exr` - OpenEXR 格式（通过 tinyexr）

#### HDR 预设配置

**HDRSkybox - 天空盒渲染**
```cpp
// 适用于等距矩形天空盒渲染
auto skybox = Texture2D::LoadFromFile("sky.exr", texture_presets::HDRSkybox());

// 特点：
// - wrap_s: Repeat（消除水平接缝）
// - wrap_t: ClampToEdge（避免极点 artifact）
// - 不生成 mipmap（全屏渲染不需要）
// - RGB16F 格式
```

**HDRIBL - IBL 环境光**
```cpp
// 适用于基于图像的光照（需要 mipmap）
auto env_map = Texture2D::LoadFromFile("env.hdr", texture_presets::HDRIBL());

// 特点：
// - 所有方向 ClampToEdge
// - 生成 mipmap（用于粗糙度采样）
// - LinearMipmapLinear 过滤
// - RGB16F 格式
```

**自定义 HDR 配置**
```cpp
auto spec = texture_presets::HDRSkybox();
spec.internal_format = TextureInternalFormat::RGB32F;  // 更高精度
spec.anisotropy = 16.0f;
auto tex = Texture2D::LoadFromFile("sky.exr", spec);
```

#### 等距矩形贴图注意事项

**接缝线问题：**
等距矩形贴图将球面展开成矩形，左右边缘（经度 0° 和 360°）需要无缝连接：
- ✅ 使用 `Repeat` 环绕模式（水平方向）
- ❌ 使用 `ClampToEdge` 会产生明显接缝
```

### 4. 动态数据更新
```cpp
// 适用于视频纹理、动态UI等场景
texture.SetData(video_frame, width, height);
```

### 5. 边界颜色
```cpp
TextureSpecification spec;
spec.wrap_s = TextureWrap::ClampToBorder;
spec.border_color[0] = 1.0f;  // R
spec.border_color[1] = 0.0f;  // G
spec.border_color[2] = 0.0f;  // B
spec.border_color[3] = 1.0f;  // A
```

## PBR 工作流集成

典型的 PBR 材质纹理加载：

```cpp
struct Material {
    Texture2D albedo;
    Texture2D normal;
    Texture2D metallic;
    Texture2D roughness;
    Texture2D ao;
    
    static Material LoadPBRMaterial(const std::string& base_path) {
        Material mat;
        mat.albedo = Texture2D::LoadFromFile(
            base_path + "_albedo.png", texture_presets::Diffuse());
        mat.normal = Texture2D::LoadFromFile(
            base_path + "_normal.png", texture_presets::Normal());
        // ... 其他贴图
        return mat;
    }
};
```



## 性能优化建议

1. **Mipmap 生成**：纹理加载后一次性生成，避免运行时生成
2. **各向异性过滤**：仅在需要的纹理上启用（如地面、墙壁）
3. **纹理压缩**：使用压缩格式（需要扩展）
4. **纹理流送**：大纹理使用渐进加载
5. **纹理图集**：合并小纹理减少绑定切换

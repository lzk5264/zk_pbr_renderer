# 现代 C++ 异常安全设计

## 核心原则：谁有能力决定如何处理，谁 catch

### 异常处理的黄金法则

```cpp
// 底层（类库）：发现问题立即抛出，不要擅自处理
// 上层（调用方）：决定恢复策略（降级/退出/重试）
```

---

## 一、底层类设计：只抛出，不捕获

### ❌ 错误示范：类内部捕获并"悄悄处理"

```cpp
class Texture2D {
public:
    static Texture2D LoadFromFile(const std::string &path) {
        try {
            auto data = stbi_load(path.c_str(), ...);
            if (!data) throw std::runtime_error("Load failed");
            return Texture2D(data);
        } catch (...) {
            // ❌ 错误：底层不知道该返回什么
            // 返回白色纹理？黑色？透明？
            return CreateWhiteTexture(); 
            // 上层根本不知道加载失败了！
        }
    }
};

// 调用方完全不知道出错了
auto tex = Texture2D::LoadFromFile("missing.png"); // 返回白色纹理
// 游戏运行时所有纹理都是白色，用户困惑...
```

### ✅ 正确示范：抛出异常，让上层决定

```cpp
class Texture2D {
public:
    static Texture2D LoadFromFile(const std::string &path) {
        unsigned char *data = stbi_load(path.c_str(), ...);
        if (!data) {
            // ✅ 立即抛出，携带详细信息
            throw TextureException("Failed to load: " + path + 
                                   "\nReason: " + stbi_failure_reason());
        }
        return Texture2D(data);
    }
};
```

**原因**：
- 底层只知道"操作失败了"
- 不知道这对应用意味着什么（致命？可降级？）
- 不知道用户期望什么（白色？黑色？崩溃？）

---

## 二、上层调用：根据业务逻辑决策

### 场景 1：必需资源 - 失败即退出

```cpp
int main() {
    try {
        // PBR 渲染器必须要有这些 shader
        auto pbr_shader = Shader("pbr.vert", "pbr.frag");
        auto skybox_shader = Shader("skybox.vert", "skybox.frag");
        
    } catch (const ShaderException &e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        std::cerr << "Cannot continue without shaders." << std::endl;
        return -1; // 直接退出
    }
    
    // 运行渲染循环...
}
```

### 场景 2：可选资源 - 失败则降级

```cpp
Texture2D LoadTextureWithFallback(const std::string &path) {
    try {
        return Texture2D::LoadFromFile(path);
    } catch (const TextureException &e) {
        std::cerr << "Warning: " << e.what() << std::endl;
        
        // 降级策略：使用默认纹理
        try {
            return Texture2D::LoadFromFile("default_white.png");
        } catch (...) {
            // 如果连默认纹理都失败，生成程序纹理
            return Texture2D::CreateSolidColor(1, 1, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }
}

// 使用示例
Material mat;
mat.diffuse = LoadTextureWithFallback("models/chair/diffuse.png");
mat.normal = LoadTextureWithFallback("models/chair/normal.png");
// 即使某些纹理缺失，模型仍能显示（用白色代替）
```

### 场景 3：批量操作 - 部分失败不影响其他

```cpp
std::vector<Model> LoadModels(const std::vector<std::string> &paths) {
    std::vector<Model> models;
    int success = 0, failed = 0;
    
    for (const auto &path : paths) {
        try {
            models.push_back(Model::LoadFromFile(path));
            success++;
        } catch (const ModelException &e) {
            std::cerr << "Skip: " << e.what() << std::endl;
            failed++;
            // 继续加载其他模型
        }
    }
    
    std::cout << "Loaded " << success << " models, " 
              << failed << " failed." << std::endl;
    return models;
}
```

### 场景 4：交互式操作 - 显示错误继续运行

```cpp
void OnUserClickExportButton() {
    try {
        ExportScene("scene.obj");
        ShowNotification("Export successful!");
    } catch (const FileException &e) {
        ShowErrorDialog("Export failed: " + std::string(e.what()));
        // 不退出程序，用户可以重试或选择其他路径
    }
}
```

---

## 三、图形程序中的异常处理检查清单

### ✅ 必须 try-catch 的操作

| 操作类型 | 原因 | 推荐策略 |
|---------|------|---------|
| **文件加载** (纹理/模型/shader) | 文件可能不存在、格式错误、权限问题 | 必需资源→退出<br>可选资源→降级 |
| **OpenGL 初始化** (GLFW/GLAD) | 驱动不支持、版本不匹配 | 失败即退出 |
| **Shader 编译** | 语法错误、版本不兼容 | 失败即退出 |
| **GPU 内存分配** (大纹理/大模型) | 显存不足 | 降低分辨率/LOD |
| **配置文件解析** (JSON/XML) | 格式错误 | 使用默认配置 |
| **用户交互** (保存/导出) | 磁盘满、路径无效 | 显示错误提示 |

### ❌ 不需要 try-catch 的情况

| 情况 | 原因 | 正确做法 |
|------|------|---------|
| **RAII 管理的资源** | 析构函数自动清理 | 让异常传播 |
| **编程逻辑错误** (数组越界/空指针) | 这是 bug | 修复代码，不要 catch |
| **构造函数调用** | 失败会自动传播 | 在更外层 catch |
| **渲染循环内部** (Draw 调用) | 失败应该崩溃以便调试 | 不 catch，让它崩溃 |

---

## 四、RAII 与异常安全

### RAII 的三大保证

```cpp
class Texture2D {
private:
    TextureHandle handle_; // RAII wrapper
    
public:
    Texture2D(const std::string &path) {
        // 即使后续抛异常，handle_ 也会自动析构
        handle_ = TextureHandle(GL_TEXTURE_2D);
        
        auto data = LoadImageData(path); // 可能抛异常
        if (!data) {
            throw TextureException("Load failed");
            // handle_ 自动析构，OpenGL 纹理对象被删除
        }
        
        UploadToGPU(data); // 可能抛异常
        // 如果失败，handle_ 仍然会析构
    }
    
    ~Texture2D() {
        // handle_ 自动析构，不需要手动 try-catch
    }
};
```

### 完整的异常安全示例

```cpp
class Material {
private:
    Texture2D diffuse_;   // RAII
    Texture2D normal_;    // RAII
    Texture2D specular_;  // RAII
    Shader shader_;       // RAII
    
public:
    Material(const std::string &diffuse_path,
             const std::string &normal_path,
             const std::string &specular_path) 
        : diffuse_(diffuse_path)     // 失败会抛出，Material 构造失败
        , normal_(normal_path)       // 如果 diffuse_ 成功但 normal_ 失败
                                     // diffuse_ 会自动析构
        , specular_(specular_path)
        , shader_("pbr.vert", "pbr.frag")
    {
        // 所有成员都构造成功才到这里
        // 如果任何一个失败，已构造的会自动析构
    }
    
    // 不需要析构函数！所有成员都是 RAII
    // ~Material() = default;
};

// 使用示例
void LoadMaterials() {
    try {
        Material mat("diffuse.png", "normal.png", "spec.png");
        // 使用 mat...
    } catch (const TextureException &e) {
        std::cerr << "Texture load failed: " << e.what() << std::endl;
        // 所有已加载的纹理都已自动释放
    } catch (const ShaderException &e) {
        std::cerr << "Shader compile failed: " << e.what() << std::endl;
        // diffuse/normal/specular 都已自动释放
    }
}
```

---

## 五、快速判断法则

### 三个问题决定是否需要 try-catch

#### 1. 这个操作依赖外部资源吗？

```cpp
// ✅ 依赖外部资源 → 需要 try-catch
auto tex = Texture2D::LoadFromFile("texture.png");  // 文件系统
auto model = Model::Load("model.obj");              // 文件系统
shader.Compile(source_code);                        // GPU 编译器

// ❌ 不依赖外部资源 → 不需要 try-catch
mesh.Draw();                          // 纯 GPU 操作
shader.SetMat4("model", matrix);      // 内存操作
```

#### 2. 失败后有合理的恢复策略吗？

```cpp
// ✅ 有恢复策略 → 需要 try-catch
Texture2D LoadDiffuse(const std::string &path) {
    try {
        return Texture2D::LoadFromFile(path);
    } catch (...) {
        return GetWhiteTexture(); // 降级策略
    }
}

// ❌ 无法恢复 → 不要 catch，让它崩溃
Shader shader("main.vert", "main.frag");
// 如果 shader 编译失败，程序无法继续
// 直接让异常传播到 main() 并退出
```

#### 3. 这是编程错误还是运行时错误？

```cpp
// ❌ 编程错误 → 修 bug，不要 catch
try {
    int value = array[10000]; // 数组越界
} catch (...) {
    // 不要这样做！这是 bug，应该修复索引计算
}

// ✅ 运行时错误 → 需要 try-catch
try {
    auto config = LoadConfig("settings.json"); // 文件可能不存在
} catch (...) {
    config = GetDefaultConfig(); // 使用默认配置
}
```

---

## 六、本项目的异常处理架构

### 异常类层次结构

```cpp
namespace zk_pbr::gfx {

// 基类
class GraphicsException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// 具体异常类型
class ShaderException : public GraphicsException {
    using GraphicsException::GraphicsException;
};

class TextureException : public GraphicsException {
    using GraphicsException::GraphicsException;
};

class MeshException : public GraphicsException {
    using GraphicsException::GraphicsException;
};

class ModelException : public GraphicsException {
    using GraphicsException::GraphicsException;
};

} // namespace zk_pbr::gfx
```

### main.cpp 的标准模式

```cpp
int main() {
    try {
        // 1. 初始化阶段 - 全部在 try 块内
        InitGLFW();
        InitGLAD();
        
        // 2. 加载必需资源
        Shader pbr_shader("pbr.vert", "pbr.frag");
        Shader skybox_shader("skybox.vert", "skybox.frag");
        
        // 3. 加载场景（可选资源用 fallback）
        auto models = LoadSceneModels();
        
        // 4. 渲染循环 - 不需要 try-catch
        while (!glfwWindowShouldClose(window)) {
            ProcessInput(window);
            Render(models, pbr_shader);
            glfwSwapBuffers(window);
        }
        
    } catch (const ShaderException &e) {
        std::cerr << "Shader Error: " << e.what() << std::endl;
        Cleanup();
        return -1;
        
    } catch (const TextureException &e) {
        std::cerr << "Texture Error: " << e.what() << std::endl;
        Cleanup();
        return -1;
        
    } catch (const std::exception &e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        Cleanup();
        return -1;
    }
    
    Cleanup();
    return 0;
}
```

---

## 七、异常安全等级（C++ 标准）

### 1. No-throw guarantee (noexcept)

```cpp
// 保证不抛异常，可以标记 noexcept
void Texture2D::Bind(uint32_t slot) const noexcept {
    if (handle_.IsValid()) {
        glActiveTexture(GL_TEXTURE0 + slot);
        handle_.Bind();
    }
}

// 析构函数必须 noexcept
~Texture2D() noexcept {
    // RAII handle 自动清理
}
```

### 2. Strong guarantee (强异常保证)

```cpp
// 要么成功，要么保持原状（事务性）
void Material::SetTexture(TextureType type, const Texture2D &texture) {
    // 复制后再赋值，如果复制失败，原 texture 不变
    Texture2D temp = texture; // 可能抛异常
    textures_[type] = std::move(temp); // noexcept
}
```

### 3. Basic guarantee (基本保证)

```cpp
// 失败后对象仍可用，但状态可能改变
void Scene::LoadModels(const std::vector<std::string> &paths) {
    for (const auto &path : paths) {
        try {
            models_.push_back(Model::LoadFromFile(path));
        } catch (...) {
            // models_ 仍然有效，但只包含部分模型
        }
    }
}
```

---

## 八、面试回答模板

### 问：你的异常处理策略是什么？

**答**：
> "我遵循'谁有能力处理，谁 catch'的原则。
> 
> 底层类（Texture、Shader）只负责发现问题并抛出带详细信息的异常，不擅自处理，因为它们不知道这个错误对应用有多严重。
> 
> 上层调用方根据业务逻辑决定：必需资源失败就退出，可选资源失败就降级使用默认值。
> 
> 所有资源都用 RAII 管理，确保异常发生时能自动清理，不会泄漏 OpenGL 对象。"

### 问：为什么不在类内部 catch 住异常？

**答**：
> "如果 Texture2D::LoadFromFile 内部 catch 住异常并返回白色纹理，调用方完全不知道加载失败了。
> 
> 但实际上，有些情况下缺失纹理是致命错误（比如 UI 纹理），有些情况下可以降级（比如装饰性贴花）。
> 
> 只有调用方知道这个纹理有多重要，所以应该让异常传播到有判断能力的层次。"

---

## 总结

| 层次 | 职责 | 操作 |
|-----|------|------|
| **底层类** | 检测错误 | `throw Exception("详细信息")` |
| **中间层** | 提供策略选择 | 提供 fallback 版本函数 |
| **上层/main** | 做出决策 | `try-catch` + 选择退出/降级/默认值 |

**核心理念**：
- **Fail-fast**：底层快速失败，不掩盖问题
- **RAII**：资源自动清理，异常安全
- **决策上移**：让有全局视角的代码做决定

这就是现代 C++ 的异常安全设计！

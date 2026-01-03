# Mesh 和 VertexLayout 使用示例

## 基本概念

这套封装系统由三部分组成：

1. **VertexLayout** - 描述顶点数据的内存布局
2. **Mesh** - 封装 VAO/VBO/EBO 的 RAII 类
3. **PrimitiveFactory** - 快速创建常用图元的工厂类

## 使用方式

### 方式 1：使用 PrimitiveFactory（最简单）

```cpp
#include <zk_pbr/gfx/mesh.h>

// 创建基本图元
auto triangle = zk_pbr::gfx::PrimitiveFactory::CreateTriangle();
auto quad = zk_pbr::gfx::PrimitiveFactory::CreateQuad();
auto cube = zk_pbr::gfx::PrimitiveFactory::CreateCube();
auto sphere = zk_pbr::gfx::PrimitiveFactory::CreateSphere(64, 32);
auto plane = zk_pbr::gfx::PrimitiveFactory::CreatePlane(10.0f, 10.0f, 10, 10);

// 绘制
shader.Use();
triangle.Draw();        // 默认 GL_TRIANGLES
sphere.Draw(GL_LINES);  // 可以指定绘制模式
```

### 方式 2：使用预定义的 VertexLayout

```cpp
#include <zk_pbr/gfx/mesh.h>
#include <zk_pbr/gfx/vertex_layout.h>

// 示例：仅位置的三角形
float vertices[] = {
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    0.0f, 0.5f, 0.0f
};

auto mesh = zk_pbr::gfx::Mesh(
    vertices, 3, 
    zk_pbr::gfx::layouts::Position3D()
);

// 示例：带纹理坐标的四边形
float vertices_tex[] = {
    // 位置              // 纹理坐标
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
    -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
};

unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

auto quad = zk_pbr::gfx::Mesh(
    vertices_tex, 4, 
    indices, 6,
    zk_pbr::gfx::layouts::Position3DTexCoord2D()
);
```

### 方式 3：自定义 VertexLayout

```cpp
// 创建自定义顶点布局
zk_pbr::gfx::VertexLayout customLayout;

// 位置属性 (location = 0, vec3)
customLayout.AddFloat(0, 3, 9 * sizeof(float), 0);

// 颜色属性 (location = 1, vec3)
customLayout.AddFloat(1, 3, 9 * sizeof(float), 3 * sizeof(float));

// 纹理坐标 (location = 2, vec3)
customLayout.AddFloat(2, 3, 9 * sizeof(float), 6 * sizeof(float));

// 自定义顶点数据
float customVertices[] = {
    // 位置              // 颜色           // 纹理坐标
    -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
     0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  0.5f, 1.0f, 0.0f
};

auto customMesh = zk_pbr::gfx::Mesh(customVertices, 3, customLayout);
```

### 方式 4：加载模型数据（未来扩展）

```cpp
// 从 Assimp 或其他模型加载器获取数据
struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
};

std::vector<ModelVertex> modelVertices = LoadModelFromFile("model.obj");
std::vector<unsigned int> modelIndices = LoadModelIndices("model.obj");

auto modelMesh = zk_pbr::gfx::Mesh(
    modelVertices.data(), 
    modelVertices.size(),
    modelIndices.data(),
    modelIndices.size(),
    zk_pbr::gfx::layouts::PBRVertex()  // 使用完整的 PBR 顶点布局
);
```

## 预定义的 VertexLayout

- `layouts::Position3D()` - 仅位置 (vec3)
- `layouts::Position3DColor3D()` - 位置 + 颜色 (vec3 + vec3)
- `layouts::Position3DTexCoord2D()` - 位置 + 纹理坐标 (vec3 + vec2)
- `layouts::Position3DNormal3DTexCoord2D()` - 位置 + 法线 + 纹理坐标 (vec3 + vec3 + vec2)
- `layouts::PBRVertex()` - 完整 PBR 顶点 (位置 + 法线 + 纹理坐标 + 切线)

## Mesh 的高级功能

### 动态更新顶点数据

```cpp
auto mesh = zk_pbr::gfx::PrimitiveFactory::CreateTriangle();

// 更新顶点数据
float newVertices[] = { /* 新的顶点数据 */ };
mesh.UpdateVertexData(newVertices, 3);

// 更新索引数据
unsigned int newIndices[] = { /* 新的索引数据 */ };
mesh.UpdateIndexData(newIndices, 6);
```

### 获取信息

```cpp
size_t vertexCount = mesh.GetVertexCount();
size_t indexCount = mesh.GetIndexCount();
GLuint vao = mesh.GetVAO();
GLuint vbo = mesh.GetVBO();
GLuint ebo = mesh.GetEBO();
```

### 移动语义（高效传递）

```cpp
// Mesh 支持移动语义，可以高效地传递所有权
zk_pbr::gfx::Mesh CreateMyMesh() {
    return zk_pbr::gfx::PrimitiveFactory::CreateCube(); // 无拷贝
}

auto mesh = CreateMyMesh(); // 移动构造
```

## 完整渲染示例

```cpp
#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/mesh.h>

int main() {
    // ... OpenGL 初始化 ...
    
    // 创建着色器和网格
    zk_pbr::gfx::Shader shader("vs.vert", "fs.frag");
    auto cube = zk_pbr::gfx::PrimitiveFactory::CreateCube();
    auto sphere = zk_pbr::gfx::PrimitiveFactory::CreateSphere();
    
    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        shader.Use();
        shader.SetMat4("model", modelMatrix);
        shader.SetMat4("view", viewMatrix);
        shader.SetMat4("projection", projectionMatrix);
        
        // 绘制立方体
        shader.SetVec3("objectColor", glm::vec3(1.0f, 0.0f, 0.0f));
        cube.Draw();
        
        // 绘制球体
        shader.SetVec3("objectColor", glm::vec3(0.0f, 1.0f, 0.0f));
        sphere.Draw();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Mesh 和 Shader 会自动清理资源
    return 0;
}
```

## 设计优势

1. **RAII 资源管理** - 无需手动 delete，自动清理 VAO/VBO/EBO
2. **灵活的顶点布局** - 支持任意顶点属性组合
3. **类型安全** - 编译期检查，避免错误
4. **易于扩展** - 可添加新的预定义布局和图元
5. **性能优化** - 支持 EBO 索引绘制，减少顶点重复
6. **统一接口** - 模型和图元使用相同的 API

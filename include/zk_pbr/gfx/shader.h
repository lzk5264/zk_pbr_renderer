#pragma once

#include <string>
#include <unordered_map>
#include <stdexcept>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace zk_pbr::gfx
{

    // Shader 编译/链接异常
    struct ShaderException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Shader 类：封装 OpenGL Shader Program (RAII)
    // 职责：加载、编译、链接 GLSL 着色器，设置 Uniform
    class Shader
    {
    public:
        // ===== 构造与析构 (RAII 核心) =====
        Shader() = default;
        Shader(const std::string &vertex_path, const std::string &fragment_path);
        ~Shader();

        // 从源码创建 (用于内嵌 shader，如 IBL 处理)
        static Shader CreateFromSource(const std::string &vs_source, const std::string &fs_source);

        // ===== 禁止拷贝，允许移动 (GPU 资源管理标配) =====
        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;
        Shader(Shader &&other) noexcept;
        Shader &operator=(Shader &&other) noexcept;

        // ===== 核心功能 =====
        void Use() const;
        GLuint GetID() const noexcept { return program_id_; }
        bool IsValid() const noexcept { return program_id_ != 0; }

        // ===== Uniform Setters =====
        void SetBool(const std::string &name, bool value) const;
        void SetInt(const std::string &name, int value) const;
        void SetFloat(const std::string &name, float value) const;
        void SetVec2(const std::string &name, const glm::vec2 &value) const;
        void SetVec3(const std::string &name, const glm::vec3 &value) const;
        void SetVec4(const std::string &name, const glm::vec4 &value) const;
        void SetMat3(const std::string &name, const glm::mat3 &mat) const;
        void SetMat4(const std::string &name, const glm::mat4 &mat) const;

        // ===== 纹理绑定 (Modern OpenGL 4.2+) =====
        // Shader 中使用 layout(binding = N) 指定绑定点
        // @param texture_id: 纹理 ID
        // @param unit: 纹理单元，对应 shader 中的 binding 值
        // @param target: 纹理目标 (GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP 等)
        static void BindTextureToUnit(GLuint texture_id, int unit, GLenum target = GL_TEXTURE_2D);

    private:
        GLuint program_id_ = 0;

        // Uniform Location 缓存
        mutable std::unordered_map<std::string, GLint> uniform_cache_;

        GLint GetUniformLocation(const std::string &name) const;

        // ===== 辅助函数 =====
        static std::string ReadFile(const std::string &path);
        static GLuint CompileShader(const std::string &source, GLenum type, const std::string &path);
        static void CheckCompileErrors(GLuint shader, GLenum type, const std::string &path);
        static void CheckLinkErrors(GLuint program);
    };

} // namespace zk_pbr::gfx
#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility> // std::exchange

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace zk_pbr::gfx
{

    struct ShaderException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    class Shader
    {
    public:
        Shader() = delete;
        Shader(const std::string &vs_path, const std::string &fs_path);

        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;

        Shader(Shader &&) noexcept = default;
        Shader &operator=(Shader &&) noexcept = default;

        ~Shader() = default;

        void Use() const noexcept;
        GLuint GetId() const noexcept;

        // Uniform 设置函数
        void SetBool(const std::string &name, bool value) const noexcept;
        void SetInt(const std::string &name, int value) const noexcept;
        void SetFloat(const std::string &name, float value) const noexcept;

        void SetVec2(const std::string &name, const glm::vec2 &value) const noexcept;
        void SetVec2(const std::string &name, float x, float y) const noexcept;

        void SetVec3(const std::string &name, const glm::vec3 &value) const noexcept;
        void SetVec3(const std::string &name, float x, float y, float z) const noexcept;

        void SetVec4(const std::string &name, const glm::vec4 &value) const noexcept;
        void SetVec4(const std::string &name, float x, float y, float z, float w) const noexcept;

        void SetMat2(const std::string &name, const glm::mat2 &mat) const noexcept;
        void SetMat3(const std::string &name, const glm::mat3 &mat) const noexcept;
        void SetMat4(const std::string &name, const glm::mat4 &mat) const noexcept;

        // ===== Texture Binding (Modern OpenGL 4.2+) =====
        // Shader 中使用 layout(binding = N) 指定绑定点，不需要运行时 glUniform1i
        // 直接绑定纹理到指定单元
        // @param textureID: 纹理 ID
        // @param unit: 纹理单元，对应 shader 中的 binding 值
        // @param target: 纹理目标（GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP 等）
        static void BindTextureToUnit(GLuint textureID, int unit, GLenum target = GL_TEXTURE_2D) noexcept;

        // ===== UBO (Uniform Buffer Object) Binding =====
        // 注意：Shader 中统一使用 layout(std140, binding = N) 声明
        // CPU 端只需调用 UniformBuffer::BindToPoint(N) 即可，无需通过 Shader 类设置

    private:
        // 独占 Program 句柄（RAII）
        class UniqueProgram
        {
        public:
            UniqueProgram() = default;
            explicit UniqueProgram(GLuint id) noexcept : id_(id) {}
            UniqueProgram(const UniqueProgram &) = delete;
            UniqueProgram &operator=(const UniqueProgram &) = delete;

            UniqueProgram(UniqueProgram &&other) noexcept : id_(std::exchange(other.id_, 0)) {}
            UniqueProgram &operator=(UniqueProgram &&other) noexcept
            {
                if (this != &other)
                    reset(std::exchange(other.id_, 0));
                return *this;
            }

            ~UniqueProgram() noexcept { reset(0); }

            GLuint get() const noexcept { return id_; }
            GLuint release() noexcept { return std::exchange(id_, 0); }
            void reset(GLuint new_id) noexcept
            {
                if (id_)
                    glDeleteProgram(id_);
                id_ = new_id;
            }

        private:
            GLuint id_ = 0;
        };

        UniqueProgram program_;

        // Uniform location 缓存（mutable 允许在 const 函数中修改）
        mutable std::unordered_map<std::string, GLint> uniform_location_cache_;

        // 获取 uniform location（带缓存）
        GLint GetUniformLocation(const std::string &name) const noexcept;

        // 工具函数
        static std::string LoadShaderSource(const std::string &file_path);

        // 编译：返回独占 Shader 句柄
        class UniqueShader;
        static UniqueShader CompileShaderFromSource(const std::string &source,
                                                    GLenum shader_type,
                                                    const std::string &debug_path);

        static std::string ShaderTypeToString(GLenum shader_type);
    };

} // namespace zk_pbr::gfx

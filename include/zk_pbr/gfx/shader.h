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
        GLuint id() const noexcept;

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

        // Texture binding
        // 绑定 2D 纹理到指定纹理单元
        // @param name: sampler uniform 名称
        // @param textureID: 纹理 ID
        // @param unit: 纹理单元（0-31）
        void SetTexture2D(const std::string &name, GLuint textureID, int unit) const noexcept;

        // 绑定 Cubemap 纹理到指定纹理单元
        void SetTextureCube(const std::string &name, GLuint textureID, int unit) const noexcept;

        // 通用纹理绑定（默认 GL_TEXTURE_2D，保持向后兼容）
        void SetTexture(const std::string &name, GLuint textureID, int unit) const noexcept;

        // UBO (Uniform Buffer Object) binding
        // 绑定 Uniform Block 到指定 binding point
        // @param blockName: Uniform Block 名称
        // @param bindingPoint: Binding point（0-based）
        void SetUniformBlock(const std::string &blockName, GLuint bindingPoint) const noexcept;

        // 获取 Uniform Block 索引
        [[nodiscard]] GLuint GetUniformBlockIndex(const std::string &blockName) const noexcept;

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

        // Uniform Block 索引缓存
        mutable std::unordered_map<std::string, GLuint> uniform_block_index_cache_;

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

#include <zk_pbr/gfx/shader.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

namespace zk_pbr::gfx
{

    class Shader::UniqueShader
    {
    public:
        UniqueShader() = default;
        UniqueShader(GLuint id, GLenum type) noexcept : id_(id), type_(type) {}

        UniqueShader(const UniqueShader &) = delete;
        UniqueShader &operator=(const UniqueShader &) = delete;

        UniqueShader(UniqueShader &&other) noexcept
            : id_(std::exchange(other.id_, 0)), type_(other.type_) {}

        UniqueShader &operator=(UniqueShader &&other) noexcept
        {
            if (this != &other)
            {
                reset(std::exchange(other.id_, 0), other.type_);
            }
            return *this;
        }

        ~UniqueShader() noexcept { reset(0, type_); }

        GLuint get() const noexcept { return id_; }
        GLuint release() noexcept { return std::exchange(id_, 0); }
        GLenum type() const noexcept { return type_; }

        void reset(GLuint new_id, GLenum new_type) noexcept
        {
            if (id_)
                glDeleteShader(id_);
            id_ = new_id;
            type_ = new_type;
        }

    private:
        GLuint id_ = 0;
        GLenum type_ = 0;
    };

    static std::string GetShaderInfoLog(GLuint shader)
    {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

        if (len <= 1)
            return {};

        std::string log(static_cast<size_t>(len), '\0');
        GLsizei written = 0;
        glGetShaderInfoLog(shader, len, &written, log.data());

        if (written > 0)
            log.resize(static_cast<size_t>(written));
        else
            log.clear();

        return log;
    }

    static std::string GetProgramInfoLog(GLuint program)
    {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

        if (len <= 1)
            return {};

        std::string log(static_cast<size_t>(len), '\0');
        GLsizei written = 0;
        glGetProgramInfoLog(program, len, &written, log.data());

        if (written > 0)
            log.resize(static_cast<size_t>(written));
        else
            log.clear();

        return log;
    }

    std::string Shader::ShaderTypeToString(GLenum shader_type)
    {
        switch (shader_type)
        {
        case GL_VERTEX_SHADER:
            return "VERTEX";
        case GL_FRAGMENT_SHADER:
            return "FRAGMENT";
        default:
            return "UNKNOWN";
        }
    }

    std::string Shader::LoadShaderSource(const std::string &file_path)
    {
        std::ifstream file(file_path, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            throw ShaderException(
                std::string("ERROR::SHADER::FILE_NOT_FOUND\nPATH: ") + file_path +
                "\nREASON: " + std::strerror(errno));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    Shader::UniqueShader Shader::CompileShaderFromSource(const std::string &source,
                                                         GLenum shader_type,
                                                         const std::string &debug_path)
    {
        GLuint id = glCreateShader(shader_type);
        if (!id)
        {
            throw ShaderException(
                std::string("ERROR::SHADER::") + ShaderTypeToString(shader_type) +
                "::CREATE_FAILED\nPATH: " + debug_path);
        }

        UniqueShader shader{id, shader_type};

        const char *cstr = source.c_str();
        glShaderSource(shader.get(), 1, &cstr, nullptr);
        glCompileShader(shader.get());

        GLint ok = 0;
        glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            std::string log = GetShaderInfoLog(shader.get());
            throw ShaderException(
                std::string("ERROR::SHADER::") + ShaderTypeToString(shader_type) +
                "::COMPILATION_FAILED\nPATH: " + debug_path + "\n" + log);
        }

        return shader; // move
    }

    Shader::Shader(const std::string &vs_path, const std::string &fs_path)
    {
        const std::string vs_src = LoadShaderSource(vs_path);
        const std::string fs_src = LoadShaderSource(fs_path);

        UniqueShader vs = CompileShaderFromSource(vs_src, GL_VERTEX_SHADER, vs_path);
        UniqueShader fs = CompileShaderFromSource(fs_src, GL_FRAGMENT_SHADER, fs_path);

        GLuint prog = glCreateProgram();
        if (!prog)
        {
            throw ShaderException("ERROR::SHADER::PROGRAM::CREATE_FAILED");
        }

        UniqueProgram program{prog};

        glAttachShader(program.get(), vs.get());
        glAttachShader(program.get(), fs.get());
        glLinkProgram(program.get());

        GLint ok = 0;
        glGetProgramiv(program.get(), GL_LINK_STATUS, &ok);
        if (!ok)
        {
            std::string log = GetProgramInfoLog(program.get());
            throw ShaderException(
                std::string("ERROR::SHADER::PROGRAM::LINKING_FAILED\n") + log);
        }

        // 更清晰/更稳：link 成功后 detach
        glDetachShader(program.get(), vs.get());
        glDetachShader(program.get(), fs.get());

        program_ = std::move(program);
    }

    void Shader::Use() const noexcept
    {
        glUseProgram(program_.get());
    }

    GLuint Shader::id() const noexcept
    {
        return program_.get();
    }

    // ======== Uniform Location 缓存实现 ========

    GLint Shader::GetUniformLocation(const std::string &name) const noexcept
    {
        // 先查缓存
        auto it = uniform_location_cache_.find(name);
        if (it != uniform_location_cache_.end())
        {
            return it->second;
        }

        // 缓存未命中，查询并缓存
        GLint location = glGetUniformLocation(program_.get(), name.c_str());
        uniform_location_cache_[name] = location;
        return location;
    }

    // ======== Uniform 设置函数实现 ========

    void Shader::SetBool(const std::string &name, bool value) const noexcept
    {
        glUniform1i(GetUniformLocation(name), static_cast<int>(value));
    }

    void Shader::SetInt(const std::string &name, int value) const noexcept
    {
        glUniform1i(GetUniformLocation(name), value);
    }

    void Shader::SetFloat(const std::string &name, float value) const noexcept
    {
        glUniform1f(GetUniformLocation(name), value);
    }

    void Shader::SetVec2(const std::string &name, const glm::vec2 &value) const noexcept
    {
        glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
    }

    void Shader::SetVec2(const std::string &name, float x, float y) const noexcept
    {
        glUniform2f(GetUniformLocation(name), x, y);
    }

    void Shader::SetVec3(const std::string &name, const glm::vec3 &value) const noexcept
    {
        glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
    }

    void Shader::SetVec3(const std::string &name, float x, float y, float z) const noexcept
    {
        glUniform3f(GetUniformLocation(name), x, y, z);
    }

    void Shader::SetVec4(const std::string &name, const glm::vec4 &value) const noexcept
    {
        glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
    }

    void Shader::SetVec4(const std::string &name, float x, float y, float z, float w) const noexcept
    {
        glUniform4f(GetUniformLocation(name), x, y, z, w);
    }

    void Shader::SetMat2(const std::string &name, const glm::mat2 &mat) const noexcept
    {
        glUniformMatrix2fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void Shader::SetMat3(const std::string &name, const glm::mat3 &mat) const noexcept
    {
        glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void Shader::SetMat4(const std::string &name, const glm::mat4 &mat) const noexcept
    {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void Shader::SetTexture2D(const std::string &name, GLuint textureID, int unit) const noexcept
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(GetUniformLocation(name), unit);
    }

    void Shader::SetTextureCube(const std::string &name, GLuint textureID, int unit) const noexcept
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
        glUniform1i(GetUniformLocation(name), unit);
    }

    void Shader::SetTexture(const std::string &name, GLuint textureID, int unit) const noexcept
    {
        // 默认使用 2D 纹理（向后兼容）
        SetTexture2D(name, textureID, unit);
    }

    void Shader::SetUniformBlock(const std::string &blockName, GLuint bindingPoint) const noexcept
    {
        GLuint blockIndex = GetUniformBlockIndex(blockName);
        if (blockIndex != GL_INVALID_INDEX)
        {
            glUniformBlockBinding(program_.get(), blockIndex, bindingPoint);
        }
    }

    GLuint Shader::GetUniformBlockIndex(const std::string &blockName) const noexcept
    {
        // 查找缓存
        auto it = uniform_block_index_cache_.find(blockName);
        if (it != uniform_block_index_cache_.end())
        {
            return it->second;
        }

        // 查询 Uniform Block 索引
        GLuint blockIndex = glGetUniformBlockIndex(program_.get(), blockName.c_str());

        // 缓存结果（即使是 GL_INVALID_INDEX 也缓存，避免重复查询）
        uniform_block_index_cache_[blockName] = blockIndex;

        return blockIndex;
    }

} // namespace zk_pbr::gfx

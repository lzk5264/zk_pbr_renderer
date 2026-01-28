#include <zk_pbr/gfx/shader.h>

#include <fstream>
#include <sstream>

#include <glm/gtc/type_ptr.hpp>

namespace zk_pbr::gfx
{

    // ===== 文件读取 =====
    std::string Shader::ReadFile(const std::string &path)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            throw ShaderException("Failed to open shader file: " + path);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // ===== 编译错误检查 =====
    void Shader::CheckCompileErrors(GLuint shader, GLenum type, const std::string &path)
    {
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            GLint log_length;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

            std::string info_log(log_length, '\0');
            glGetShaderInfoLog(shader, log_length, nullptr, info_log.data());

            std::string type_str = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
            throw ShaderException("Shader compilation failed [" + type_str + "]: " + path + "\n" + info_log);
        }
    }

    // ===== 链接错误检查 =====
    void Shader::CheckLinkErrors(GLuint program)
    {
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);

        if (!success)
        {
            GLint log_length;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

            std::string info_log(log_length, '\0');
            glGetProgramInfoLog(program, log_length, nullptr, info_log.data());

            throw ShaderException("Shader program linking failed:\n" + info_log);
        }
    }

    // ===== 编译单个 Shader =====
    GLuint Shader::CompileShader(const std::string &source, GLenum type, const std::string &path)
    {
        GLuint shader = glCreateShader(type);
        if (shader == 0)
        {
            throw ShaderException("Failed to create shader object");
        }

        const char *src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        try
        {
            CheckCompileErrors(shader, type, path);
        }
        catch (...)
        {
            glDeleteShader(shader); // 编译失败，清理资源
            throw;
        }

        return shader;
    }

    // ===== 从源码创建 =====
    Shader Shader::CreateFromSource(const std::string &vs_source, const std::string &fs_source)
    {
        Shader shader;

        GLuint vs = CompileShader(vs_source, GL_VERTEX_SHADER, "[inline]");
        GLuint fs = 0;
        try
        {
            fs = CompileShader(fs_source, GL_FRAGMENT_SHADER, "[inline]");
        }
        catch (...)
        {
            glDeleteShader(vs);
            throw;
        }

        shader.program_id_ = glCreateProgram();
        glAttachShader(shader.program_id_, vs);
        glAttachShader(shader.program_id_, fs);
        glLinkProgram(shader.program_id_);

        glDeleteShader(vs);
        glDeleteShader(fs);

        try
        {
            CheckLinkErrors(shader.program_id_);
        }
        catch (...)
        {
            glDeleteProgram(shader.program_id_);
            shader.program_id_ = 0;
            throw;
        }

        return shader;
    }

    // ===== 构造函数 =====
    Shader::Shader(const std::string &vertex_path, const std::string &fragment_path)
    {
        // 1. 读取文件
        std::string vs_source = ReadFile(vertex_path);
        std::string fs_source = ReadFile(fragment_path);

        // 2. 编译 Vertex Shader
        GLuint vs = CompileShader(vs_source, GL_VERTEX_SHADER, vertex_path);

        // 3. 编译 Fragment Shader (如果失败，需要清理 vs)
        GLuint fs = 0;
        try
        {
            fs = CompileShader(fs_source, GL_FRAGMENT_SHADER, fragment_path);
        }
        catch (...)
        {
            glDeleteShader(vs);
            throw;
        }

        // 4. 创建并链接 Program
        program_id_ = glCreateProgram();
        glAttachShader(program_id_, vs);
        glAttachShader(program_id_, fs);
        glLinkProgram(program_id_);

        // 5. 链接完成后可以删除 Shader 对象
        // (这是面试考点：link 后 shader 对象就没用了)
        glDeleteShader(vs);
        glDeleteShader(fs);

        // 6. 检查链接错误
        try
        {
            CheckLinkErrors(program_id_);
        }
        catch (...)
        {
            glDeleteProgram(program_id_);
            program_id_ = 0;
            throw;
        }
    }

    // ===== 析构函数 =====
    Shader::~Shader()
    {
        if (program_id_ != 0)
        {
            glDeleteProgram(program_id_);
        }
    }

    // ===== 移动构造 =====
    Shader::Shader(Shader &&other) noexcept
        : program_id_(other.program_id_),
          uniform_cache_(std::move(other.uniform_cache_))
    {
        other.program_id_ = 0;
    }

    // ===== 移动赋值 =====
    Shader &Shader::operator=(Shader &&other) noexcept
    {
        if (this != &other)
        {
            // 先释放自己的资源
            if (program_id_ != 0)
            {
                glDeleteProgram(program_id_);
            }

            // 接管 other 的资源
            program_id_ = other.program_id_;
            uniform_cache_ = std::move(other.uniform_cache_);

            other.program_id_ = 0;
        }
        return *this;
    }

    // ===== 使用 Shader =====
    void Shader::Use() const
    {
        glUseProgram(program_id_);
    }

    // ===== Uniform Location 缓存 =====
    GLint Shader::GetUniformLocation(const std::string &name) const
    {
        auto it = uniform_cache_.find(name);
        if (it != uniform_cache_.end())
        {
            return it->second;
        }

        GLint location = glGetUniformLocation(program_id_, name.c_str());
        uniform_cache_[name] = location;
        return location;
    }

    // ===== Uniform Setters =====

    void Shader::SetBool(const std::string &name, bool value) const
    {
        glUniform1i(GetUniformLocation(name), static_cast<int>(value));
    }

    void Shader::SetInt(const std::string &name, int value) const
    {
        glUniform1i(GetUniformLocation(name), value);
    }

    void Shader::SetFloat(const std::string &name, float value) const
    {
        glUniform1f(GetUniformLocation(name), value);
    }

    void Shader::SetVec2(const std::string &name, const glm::vec2 &value) const
    {
        glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
    }

    void Shader::SetVec3(const std::string &name, const glm::vec3 &value) const
    {
        glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
    }

    void Shader::SetVec4(const std::string &name, const glm::vec4 &value) const
    {
        glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
    }

    void Shader::SetMat3(const std::string &name, const glm::mat3 &mat) const
    {
        glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void Shader::SetMat4(const std::string &name, const glm::mat4 &mat) const
    {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    // ===== 纹理绑定 =====
    void Shader::BindTextureToUnit(GLuint texture_id, int unit, GLenum target)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(target, texture_id);
    }

} // namespace zk_pbr::gfx
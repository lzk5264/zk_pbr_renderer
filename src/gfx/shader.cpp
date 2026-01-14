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

    Shader Shader::CreateFromSource(const std::string &vs_source, const std::string &fs_source)
    {
        // 编译顶点着色器
        auto vs = CompileShaderFromSource(vs_source, GL_VERTEX_SHADER, "[inline_vs]");

        // 编译片段着色器
        auto fs = CompileShaderFromSource(fs_source, GL_FRAGMENT_SHADER, "[inline_fs]");

        // 链接程序
        GLuint program_id = glCreateProgram();
        glAttachShader(program_id, vs.get());
        glAttachShader(program_id, fs.get());
        glLinkProgram(program_id);

        // 检查链接错误
        GLint success = 0;
        glGetProgramiv(program_id, GL_LINK_STATUS, &success);

        if (!success)
        {
            std::string log = GetProgramInfoLog(program_id);
            glDeleteProgram(program_id);
            throw ShaderException(
                std::string("ERROR::SHADER::PROGRAM::LINK_FAILED\n") +
                "SOURCE: [inline shader]\n" +
                log);
        }

        // 创建 Shader 对象并直接设置 program
        Shader shader;
        shader.program_.reset(program_id);
        return shader;
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

    GLuint Shader::GetId() const noexcept
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

    void Shader::BindTextureToUnit(GLuint textureID, int unit, GLenum target) noexcept
    {
        // 直接绑定纹理到纹理单元，不设置 uniform
        // Shader 中使用 layout(binding = N) uniform sampler2D texName;
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(target, textureID);
    }

    // UBO 绑定说明：
    // Shader 中统一使用 layout(std140, binding = N) 声明 Uniform Block
    // CPU 端只需调用 UniformBuffer::BindToPoint(N) 将 UBO 绑定到对应的 binding point
    // 不需要通过 Shader 类进行运行时绑定

} // namespace zk_pbr::gfx

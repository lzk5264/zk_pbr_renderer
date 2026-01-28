#include <zk_pbr/gfx/texture_cubemap.h>
#include <zk_pbr/gfx/texture.h>
#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/uniform_buffer.h>
#include <zk_pbr/gfx/ubo_definitions.h>
#include <zk_pbr/gfx/mesh.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace zk_pbr::gfx
{

    // ========== 内部辅助 ==========
    namespace
    {
        // Cubemap 6 个面的视图矩阵
        const glm::mat4 kCubemapViews[6] = {
            glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

        // 共享的 Cubemap 渲染顶点着色器
        const char *kCubemapVS = R"(
#version 460 core
layout(location = 0) in vec3 a_Position;
out vec3 v_LocalPos;

layout(std140, binding = 15) uniform CaptureMatrices {
    mat4 u_View;
    mat4 u_Projection;
};

void main() {
    v_LocalPos = a_Position;
    gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}
)";

        // Equirectangular -> Cubemap 转换片段着色器
        const char *kEquirectToCubemapFS = R"(
#version 460 core
in vec3 v_LocalPos;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_EquirectMap;

const float PI = 3.14159265359;

void main() {
    vec3 dir = normalize(v_LocalPos);
    
    // 球面坐标转 UV
    float phi = atan(dir.z, dir.x);
    float theta = asin(dir.y);
    vec2 uv = vec2(phi / (2.0 * PI) + 0.5, theta / PI + 0.5);
    
    FragColor = vec4(texture(u_EquirectMap, uv).rgb, 1.0);
}
)";

        // Irradiance 卷积片段着色器 (蒙特卡洛采样)
        const char *kIrradianceFS = R"(
#version 460 core
in vec3 v_LocalPos;
out vec4 FragColor;

layout(binding = 0) uniform samplerCube u_EnvMap;
uniform int u_SampleCount;

const float PI = 3.14159265359;

// PCG 随机数生成
uint PCGHash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float RandomFloat(inout uint seed) {
    seed = PCGHash(seed);
    return float(seed) / float(0xffffffffu);
}

// 余弦加权半球采样
vec3 CosineSampleHemisphere(float u1, float u2) {
    float r = sqrt(u1);
    float theta = 2.0 * PI * u2;
    return vec3(r * cos(theta), r * sin(theta), sqrt(1.0 - u1));
}

void main() {
    vec3 N = normalize(v_LocalPos);
    
    // 构建 TBN 矩阵
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = cross(N, right);
    
    vec3 irradiance = vec3(0.0);
    uint seed = uint(gl_FragCoord.x * 1973.0 + gl_FragCoord.y * 9277.0);
    
    for (int i = 0; i < u_SampleCount; i++) {
        float u1 = RandomFloat(seed);
        float u2 = RandomFloat(seed);
        
        vec3 localDir = CosineSampleHemisphere(u1, u2);
        vec3 worldDir = localDir.x * right + localDir.y * up + localDir.z * N;
        
        irradiance += texture(u_EnvMap, worldDir).rgb;
    }
    
    // PDF = cos(theta) / PI, 所以结果要乘以 PI
    FragColor = vec4(PI * irradiance / float(u_SampleCount), 1.0);
}
)";

        // 渲染到 Cubemap 的 6 个面
        void RenderToCubemapFaces(TextureCubemap &target, Shader &shader, int size,
                                  void (*pre_render)(Shader &, int) = nullptr)
        {
            Framebuffer fbo(size, size);
            auto cube = PrimitiveFactory::CreateCube();

            glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

            UniformBuffer ubo(sizeof(CameraUBOData), GL_DYNAMIC_DRAW);
            ubo.BindToPoint(15); // binding = 15

            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);

            fbo.Bind();
            glViewport(0, 0, size, size);
            shader.Use();

            for (int i = 0; i < 6; ++i)
            {
                fbo.AttachCubemapFace(target.GetID(), i);

                CameraUBOData data;
                data.view = kCubemapViews[i];
                data.projection = projection;
                ubo.SetData(&data, sizeof(data));

                if (pre_render)
                    pre_render(shader, i);

                glClear(GL_COLOR_BUFFER_BIT);
                cube.Draw();
            }

            fbo.Unbind();
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        }

    } // namespace

    // ========== TextureCubemap 实现 ==========

    TextureCubemap::TextureCubemap(int size, const TextureSpec &spec)
        : size_(size), spec_(spec)
    {
        glGenTextures(1, &id_);
        if (id_ == 0)
        {
            throw TextureException("Failed to create cubemap texture");
        }

        glBindTexture(GL_TEXTURE_CUBE_MAP, id_);

        // 为 6 个面分配存储
        for (int i = 0; i < 6; ++i)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                         spec.internal_format, size, size, 0,
                         spec.format, spec.data_type, nullptr);
        }

        ApplyParameters();
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    TextureCubemap::~TextureCubemap()
    {
        if (id_ != 0)
        {
            glDeleteTextures(1, &id_);
        }
    }

    TextureCubemap::TextureCubemap(TextureCubemap &&other) noexcept
        : id_(other.id_), size_(other.size_), spec_(other.spec_)
    {
        other.id_ = 0;
        other.size_ = 0;
    }

    TextureCubemap &TextureCubemap::operator=(TextureCubemap &&other) noexcept
    {
        if (this != &other)
        {
            if (id_ != 0)
            {
                glDeleteTextures(1, &id_);
            }

            id_ = other.id_;
            size_ = other.size_;
            spec_ = other.spec_;

            other.id_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    TextureCubemap TextureCubemap::LoadFromEquirectangular(
        const std::string &path, int cubemap_size, const TextureSpec &spec)
    {
        // 1. 加载 HDR 图像
        HDRImageData hdr = LoadHDRImage(path);

        // 2. 创建临时 2D 纹理
        GLuint equirect_tex;
        glGenTextures(1, &equirect_tex);
        glBindTexture(GL_TEXTURE_2D, equirect_tex);

        GLenum format = (hdr.channels == 4) ? GL_RGBA : GL_RGB;
        GLenum internal = (hdr.channels == 4) ? GL_RGBA16F : GL_RGB16F;

        glTexImage2D(GL_TEXTURE_2D, 0, internal, hdr.width, hdr.height, 0,
                     format, GL_FLOAT, hdr.pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // 3. 创建转换 Shader
        Shader convert_shader = Shader::CreateFromSource(kCubemapVS, kEquirectToCubemapFS);

        // 4. 创建目标 Cubemap
        TextureCubemap cubemap(cubemap_size, spec);

        // 5. 绑定输入纹理并渲染
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, equirect_tex);

        RenderToCubemapFaces(cubemap, convert_shader, cubemap_size);

        // 6. 清理临时纹理
        glDeleteTextures(1, &equirect_tex);

        // 7. 生成 mipmap
        if (spec.generate_mipmaps)
        {
            cubemap.GenerateMipmaps();
        }

        return cubemap;
    }

    TextureCubemap TextureCubemap::ConvolveIrradiance(
        const TextureCubemap &source, int irradiance_size, int sample_count, const TextureSpec &spec)
    {
        if (!source.IsValid())
        {
            throw TextureException("Source cubemap is invalid");
        }

        // 1. 创建卷积 Shader
        Shader irradiance_shader = Shader::CreateFromSource(kCubemapVS, kIrradianceFS);

        // 2. 创建目标 Cubemap
        TextureCubemap irradiance(irradiance_size, spec);

        // 3. 绑定源 Cubemap
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, source.GetID());

        // 4. 设置采样数并渲染
        irradiance_shader.Use();
        irradiance_shader.SetInt("u_SampleCount", sample_count);

        RenderToCubemapFaces(irradiance, irradiance_shader, irradiance_size);

        return irradiance;
    }

    void TextureCubemap::Bind(unsigned int slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
    }

    void TextureCubemap::GenerateMipmaps()
    {
        if (id_ != 0)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }
    }

    void TextureCubemap::ApplyParameters()
    {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, static_cast<GLint>(spec_.wrap));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, static_cast<GLint>(spec_.wrap));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, static_cast<GLint>(spec_.wrap));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(spec_.min_filter));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(spec_.mag_filter));
    }

} // namespace zk_pbr::gfx

#include <zk_pbr/gfx/texture_cubemap.h>
#include <zk_pbr/gfx/image_loader.h>
#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/uniform_buffer.h>
#include <zk_pbr/gfx/render_constants.h>
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

            in VS_OUT {
                vec3 uv;
            } fs_in;

            layout(location = 0) out vec4 o_Color;
            layout(binding = 0) uniform samplerCube u_Cubemap;

            layout(location = 0) uniform int u_Samplers;


            const float PI = 3.14159265359;
            const float PiOver4 = 0.7853981633974483; // Π / 4
            const float PiOver2 = 1.5707963267948966; // Π / 2


            // Hammersley
            float RadicalInverse_VdC(uint bits) 
            {
                bits = (bits << 16u) | (bits >> 16u);
                bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                return float(bits) * 2.3283064365386963e-10; // / 0x100000000
            }

            vec2 Hammersley(uint i, uint N)
            {
                return vec2(float(i)/float(N + 1), RadicalInverse_VdC(i));
            } 


            // 输入 n 必须是单位向量
            // 输出 b1, b2 与 n 构成正交基：{b1, b2, n}
            void GetONB(in vec3 n, out vec3 b1, out vec3 b2)
            {
                float s = mix(-1.0, 1.0, step(0.0, n.z)); // n.z>=0 -> 1, 否则 -1

                float a = -1.0 / (s + n.z);
                float b = n.x * n.y * a;

                b1 = vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
                b2 = vec3(b, s + n.y * n.y * a,  -n.y);
            }

            vec2 SampleUniformDiskConcentric(vec2 u)
            {
                vec2 uOffset = 2 * u - vec2(1.0f, 1.0f);

                if (uOffset.x == 0 && uOffset.y == 0)
                    return vec2(0.0f, 0.0f);


                float theta, r;
                if (abs(uOffset.x) > abs(uOffset.y)) 
                {
                    r = uOffset.x;
                    theta = PiOver4 * (uOffset.y / uOffset.x);
                } 
                else 
                {
                    r = uOffset.y;
                    theta = PiOver2 - PiOver4 * (uOffset.x / uOffset.y);
                }
                return r * vec2(cos(theta), sin(theta));
            }

            vec3 SampleCosineHemisphere(vec2 u)
            {
                vec2 d = SampleUniformDiskConcentric(u);
                float z = sqrt(1 - d.x * d.x - d.y * d.y);
                return vec3(d.x, d.y, z);
            }


            void main()
            {
                uvec2 pix = uvec2(gl_FragCoord.xy);
                uint frame = 0u; // 如果你有时间累积/降噪，就传进来；没有就保持 0

                vec3 N = normalize(fs_in.uv);
                vec3 B1, B2;
                GetONB(N, B1, B2);


                vec3 sum = vec3(0.0);
                for (int i = 0; i < u_Samplers; i ++)
                {
                    vec2 Xi = Hammersley(i, u_Samplers);
                    vec3 L = SampleCosineHemisphere(Xi);
                    L = normalize(L.x * B1 + L.y * B2 + L.z * N);

                    vec3 Li = texture(u_Cubemap, L).rgb;
                    sum += Li;
                }

                o_Color = vec4(PI * sum / float(u_Samplers), 1.0);
            }
            )";

        const char *kPrefilteredEnvMapFS = R"(
                #version 460 core

                // ===== Fragment Inputs =====
                in vec3 v_LocalPos; 

                // ===== Fragment Outputs =====
                layout(location = 0) out vec4 o_Color;

                // ===== Resources (keep bindings stable across shaders) =====

                layout(binding = 0) uniform samplerCube u_EnvMap;  

                // ===== Uniforms (small scalar params; use UBO if grows) =====
                layout(location = 0) uniform int u_SampleCount;
                layout(location = 1) uniform float u_Roughness;

                // ===== Helpers =====

                const float PI = 3.1415926;


                // Hammersley
                float RadicalInverse_VdC(uint bits) 
                {
                    bits = (bits << 16u) | (bits >> 16u);
                    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
                }

                vec2 Hammersley(uint i, uint N)
                {
                    return vec2(float(i)/float(N + 1), RadicalInverse_VdC(i));
                } 

                // coord base N -> (b1, b2, N)
                void GetONB(in vec3 n, out vec3 b1, out vec3 b2)
                {
                    float s = mix(-1.0, 1.0, step(0.0, n.z)); // n.z>=0 -> 1, 否则 -1

                    float a = -1.0 / (s + n.z);
                    float b = n.x * n.y * a;

                    b1 = vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
                    b2 = vec3(b, s + n.y * n.y * a,  -n.y);
                }

                vec3 ImportanceSampleGGX(vec2 Xi, float a2, vec3 N)
                {
                    float phi = 2 * PI * Xi.x;
                    float cos_theta = sqrt((1 - Xi.y) / (1 + (a2 - 1) * Xi.y));
                    float sin_theta = sqrt(1 - cos_theta * cos_theta);
                    
                    vec3 H = vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

                    vec3 b1, b2;
                    GetONB(N, b1, b2);
                    // TS -> WS
                    return b1 * H.x + b2 * H.y + N * H.z;
                }

                void main()
                {
                    vec3 N = normalize(v_LocalPos);

                    // 完美镜面反射，无需多次采样
                    if (u_Roughness < 1e-4) 
                    {
                        o_Color = vec4(texture(u_EnvMap, N).rgb, 1.0);
                        return;
                    }

                    // mipmap filtered samples
                    ivec2 env_map_size = textureSize(u_EnvMap, 0);
                    int W = env_map_size.x;
                    float Omega_p = 4.0 * PI / (6.0 * float(W) * float(W));
                    int mipCount = textureQueryLevels(u_EnvMap);
                    float maxMip = float(mipCount - 1);



                    float total_weight = 0.0;
                    vec3 sum = vec3(0.0);

                    float a  = u_Roughness * u_Roughness;   // alpha
                    float a2 = a * a;                        // alpha * alpha
                    for (int i = 0; i < u_SampleCount; i ++ )
                    {
                        vec2 Xi = Hammersley(i, u_SampleCount);
                        vec3 H = ImportanceSampleGGX(Xi, a2, N);
                        vec3 L = reflect(-N, H);
                        float NdotL = max(0.0, dot(N, L));
                        // 跳过背面采样
                        if (NdotL > 0.0)
                        {
                            float NdotH = max(0.0, dot(N, H));
                            float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
                            float pdf_li = a2 / (4.0 * PI * denom * denom);
                            float Omega_s = 1.0 / (float(u_SampleCount) * pdf_li + 1e-6);
                            float mip_level = clamp(0.5 * log2(Omega_s / Omega_p), 0.0, maxMip);
                            sum += textureLod(u_EnvMap, L, mip_level).rgb * NdotL;
                            total_weight += NdotL;
                        }
                    }
                        
                    float invW = 1.0 / max(total_weight, 1e-6);
                    o_Color = vec4(sum * invW, 1.0);
                }
            )";

        // 渲染到 Cubemap 的 6 个面
        void RenderToCubemapFaces(TextureCubemap &target, Shader &shader, int size, int mip_level = 0,
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
                fbo.AttachCubemapFace(target.GetID(), i, GL_COLOR_ATTACHMENT0, mip_level);

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

        int mip_levels = spec.generate_mipmaps
                             ? static_cast<int>(std::floor(std::log2(size))) + 1
                             : 1;

        glTexStorage2D(GL_TEXTURE_CUBE_MAP, mip_levels, spec.internal_format, size, size);

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

    TextureCubemap TextureCubemap::PrefilteredEnvMap(
        const TextureCubemap &source,
        int size,
        int sample_count,
        const TextureSpec &spec)
    {
        if (!source.IsValid())
        {
            throw TextureException("Source cubemap is invalid");
        }

        // 1. 创建 Shader
        Shader prefilered_shader = Shader::CreateFromSource(kCubemapVS, kPrefilteredEnvMapFS);
        // 2. 创建目标 Cubemap
        TextureCubemap prefiltered_env_map(size, spec);

        // 3. 绑定源 Cubemap
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, source.GetID());

        // 4. 设置采样数并渲染
        prefilered_shader.Use();
        prefilered_shader.SetInt("u_SampleCount", sample_count);

        int max_mip_levels = static_cast<int>(std::floor(std::log2(size) + 1));
        // size=256 → log2(256)=8 → 8+1=9 级 (256, 128, 64, 32, 16, 8, 4, 2, 1)

        for (int mip = 0; mip < max_mip_levels; mip++)
        {
            float roughness = static_cast<float>(mip) / static_cast<float>(max_mip_levels - 1);
            int mip_size = size >> mip;
            prefilered_shader.SetFloat("u_Roughness", roughness);
            RenderToCubemapFaces(prefiltered_env_map, prefilered_shader, mip_size, mip);
        }

        return prefiltered_env_map;
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

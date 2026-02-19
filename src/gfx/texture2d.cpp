#include <zk_pbr/gfx/texture2d.h>
#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/mesh.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>

namespace zk_pbr::gfx
{
    // ========== 内部辅助 ==========
    namespace
    {
        const char *kTex2DVS = R"(
                #version 460 core

                // ===== Vertex Inputs =====
                layout(location = 0) in vec3 a_PositionOS;

                void main()
                {
                    gl_Position = vec4(a_PositionOS, 1.0); // standard pipeline
                }
            )";
        const char *kBRDFLUTFS = R"(
            #version 460 core


            // ===== Fragment Outputs =====
            layout(location = 0) out vec4 o_Color;

            // ===== Uniforms (small scalar params; use UBO if grows) =====
            layout(location = 0) uniform int u_SampleCount;
            layout(location = 1) uniform int u_LUTSize;

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

            // N 固定为 (0,0,1)，切线空间即世界空间，无需 ONB 变换
            vec3 ImportanceSampleGGX(vec2 Xi, float a2)
            {
                float phi = 2.0 * PI * Xi.x;
                float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
                float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
                return vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
            }

            float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2) 
            {
                float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
                float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
                return 0.5 / (GGXV + GGXL + 1e-7);
            }


            void main()
            {
                vec2 uv = gl_FragCoord.xy / float(u_LUTSize);

                // x 轴: NdotV,  y 轴: perceptual roughness
                float NdotV = max(uv.x, 1e-4);  // 避免 NdotV=0 时 V 退化
                float roughness = uv.y;
                float a  = roughness * roughness;
                float a2 = a * a;

                // 固定 N = (0,0,1)，从 NdotV 反推 V（取 V.y = 0 的解）
                vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

                float dfg1 = 0.0, dfg2 = 0.0;

                for (int i = 0; i < u_SampleCount; i++)
                {
                    vec2 Xi = Hammersley(uint(i), uint(u_SampleCount));
                    vec3 H = ImportanceSampleGGX(Xi, a2);
                    vec3 L = reflect(-V, H);

                    float NdotL = L.z;  // N = (0,0,1)
                    if (NdotL > 0.0)
                    {
                        float NdotH = max(H.z, 0.0);
                        float VdotH = max(dot(V, H), 0.0);

                        float Vis   = V_SmithGGXCorrelated(NdotV, NdotL, a2);
                        float Fc    = pow(1.0 - VdotH, 5.0);
                        float weight = Vis * VdotH * NdotL / (NdotH + 1e-7);

                        dfg1 += (1.0 - Fc) * weight;
                        dfg2 += Fc * weight;
                    }
                }

                dfg1 = 4.0 * dfg1 / float(u_SampleCount);
                dfg2 = 4.0 * dfg2 / float(u_SampleCount);

                o_Color = vec4(dfg1, dfg2, 0.0, 1.0);
            }
        )";

        const char *kBRDFLUTMSFS = R"(
            #version 460 core


            // ===== Fragment Outputs =====
            layout(location = 0) out vec4 o_Color;

            // ===== Uniforms (small scalar params; use UBO if grows) =====
            layout(location = 0) uniform int u_SampleCount;
            layout(location = 1) uniform int u_LUTSize;

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

            // N 固定为 (0,0,1)，切线空间即世界空间，无需 ONB 变换
            vec3 ImportanceSampleGGX(vec2 Xi, float a2)
            {
                float phi = 2.0 * PI * Xi.x;
                float cos_theta = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
                float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
                return vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
            }

            float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2) 
            {
                float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
                float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
                return 0.5 / (GGXV + GGXL + 1e-7);
            }


            void main()
            {
                vec2 uv = gl_FragCoord.xy / float(u_LUTSize);

                // x 轴: NdotV,  y 轴: perceptual roughness
                float NdotV = max(uv.x, 1e-4);  // 避免 NdotV=0 时 V 退化
                float roughness = uv.y;
                float a  = roughness * roughness;
                float a2 = a * a;

                // 固定 N = (0,0,1)，从 NdotV 反推 V（取 V.y = 0 的解）
                vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

                float dfg1 = 0.0, dfg2 = 0.0;

                for (int i = 0; i < u_SampleCount; i++)
                {
                    vec2 Xi = Hammersley(uint(i), uint(u_SampleCount));
                    vec3 H = ImportanceSampleGGX(Xi, a2);
                    vec3 L = reflect(-V, H);

                    float NdotL = L.z;  // N = (0,0,1)
                    if (NdotL > 0.0)
                    {
                        float NdotH = max(H.z, 0.0);
                        float VdotH = max(dot(V, H), 0.0);

                        float Vis   = V_SmithGGXCorrelated(NdotV, NdotL, a2);
                        float Fc    = pow(1.0 - VdotH, 5.0);
                        float weight = Vis * VdotH * NdotL / (NdotH + 1e-7);

                        dfg1 += Fc * weight;
                        dfg2 += weight;
                    }
                }

                dfg1 = 4.0 * dfg1 / float(u_SampleCount);
                dfg2 = 4.0 * dfg2 / float(u_SampleCount);

                o_Color = vec4(dfg1, dfg2, 0.0, 1.0);
            }    
        )";

        // 渲染到 2D tex
        void RenderToTex2D(Texture2D &target, Shader &shader, int width, int height)
        {
            auto quad = PrimitiveFactory::CreateQuad();
            Framebuffer fbo(width, height);

            fbo.AttachTexture(target.GetID(), GL_COLOR_ATTACHMENT0);
            fbo.CheckStatus();

            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);

            fbo.Bind();
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT);
            shader.Use();
            quad.Draw();
            fbo.Unbind();

            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        }
    }

    Texture2D::Texture2D(int width, int height, const TextureSpec &spec)
        : width_(width), height_(height)
    {
        glGenTextures(1, &id_);
        if (id_ == 0)
        {
            throw TextureException("Failed to create texture");
        }

        glBindTexture(GL_TEXTURE_2D, id_);

        // 分配存储
        glTexImage2D(GL_TEXTURE_2D, 0, spec.internal_format,
                     width, height, 0, spec.format, spec.data_type, nullptr);

        // 设置参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(spec.wrap));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(spec.wrap));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(spec.min_filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(spec.mag_filter));

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Texture2D::~Texture2D()
    {
        if (id_ != 0)
        {
            glDeleteTextures(1, &id_);
        }
    }

    Texture2D::Texture2D(Texture2D &&other) noexcept
        : id_(other.id_), width_(other.width_), height_(other.height_)
    {
        other.id_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }

    Texture2D &Texture2D::operator=(Texture2D &&other) noexcept
    {
        if (this != &other)
        {
            if (id_ != 0)
            {
                glDeleteTextures(1, &id_);
            }

            id_ = other.id_;
            width_ = other.width_;
            height_ = other.height_;

            other.id_ = 0;
            other.width_ = 0;
            other.height_ = 0;
        }
        return *this;
    }

    Texture2D Texture2D::ComputeDFG(
        int size,
        int sample_count,
        bool is_multiscatter,
        const TextureSpec &spec)
    {
        // 1. 创建 Shader
        Shader dfg_shader;
        if (is_multiscatter)
            dfg_shader = Shader::CreateFromSource(kTex2DVS, kBRDFLUTMSFS);
        else
            dfg_shader = Shader::CreateFromSource(kTex2DVS, kBRDFLUTFS);
        // 2. 创建目标 tex2D
        Texture2D dfg_lut_tex(size, size, spec);
        // 3. 设置参数并渲染
        dfg_shader.Use();
        dfg_shader.SetInt("u_SampleCount", sample_count);
        dfg_shader.SetInt("u_LUTSize", size);
        RenderToTex2D(dfg_lut_tex, dfg_shader, size, size);
        return dfg_lut_tex;
    }

    void Texture2D::Bind(unsigned int slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, id_);
    }

    // DEBUG: RG16F -> PPM (R=dfg1, G=dfg2, B=0)
    void Texture2D::SaveToPPM(const std::string &path) const
    {
        if (!IsValid())
            return;

        int w = width_, h = height_;
        std::vector<float> pixels(w * h * 4); // RGBA float
        glBindTexture(GL_TEXTURE_2D, id_);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        std::ofstream f(path, std::ios::binary);
        f << "P6\n"
          << w << " " << h << "\n255\n";
        for (int y = h - 1; y >= 0; --y) // OpenGL bottom-up -> top-down
        {
            for (int x = 0; x < w; ++x)
            {
                int idx = (y * w + x) * 4;
                auto to_byte = [](float v) -> unsigned char
                {
                    return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
                };
                unsigned char rgb[3] = {to_byte(pixels[idx]), to_byte(pixels[idx + 1]), to_byte(pixels[idx + 2])};
                f.write(reinterpret_cast<char *>(rgb), 3);
            }
        }
    }

} // namespace zk_pbr::gfx

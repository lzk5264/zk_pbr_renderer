#include <zk_pbr/gfx/texture_cubemap.h>

#include <algorithm>
#include <functional>

#include <zk_pbr/gfx/shader.h>
#include <zk_pbr/gfx/framebuffer.h>
#include <zk_pbr/gfx/uniform_buffer.h>
#include <zk_pbr/gfx/ubo_definitions.h>
#include <zk_pbr/gfx/mesh.h>
#include <zk_pbr/gfx/texture2d.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace zk_pbr::gfx
{

    // ========== 内部辅助：渲染到 Cubemap 的公共逻辑 ==========
    namespace
    {
        // 6 个面的视图矩阵（共享）
        const glm::mat4 kCubemapViews[6] = {
            glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

        // 共享的立方体顶点着色器（使用 binding 15 避免冲突）
        const char *kCubemapVS = R"(
            #version 460 core
            layout(location = 0) in vec3 a_PositionOS;
            out vec3 vs_posOS;
            layout(std140, binding = 15) uniform InternalCameraUBO {
                mat4 u_View;
                mat4 u_Proj;
            };
            void main() {
                vs_posOS = a_PositionOS;
                gl_Position = u_Proj * u_View * vec4(a_PositionOS, 1.0);
            }
        )";

        // 渲染到 Cubemap 的通用配置
        struct RenderToCubemapConfig
        {
            TextureCubemap &target_cubemap;
            Shader &shader;
            int face_size;

            // 每帧渲染前的回调（可用于设置额外的 uniform 等）
            // 参数：shader, face_index
            std::function<void(Shader &, int)> on_before_face_render = nullptr;
        };

        // 通用的渲染到 Cubemap 函数
        void RenderToCubemapFaces(const RenderToCubemapConfig &config)
        {
            // 创建临时 FBO 和几何体
            Framebuffer fbo(config.face_size, config.face_size);
            auto cube = PrimitiveFactory::CreateCube();

            glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

            // 使用 kInternal binding point
            UniformBuffer internal_ubo(sizeof(CameraUBOData), GL_DYNAMIC_DRAW);
            internal_ubo.BindToPoint(ubo_binding::kInternal);

            // 保存当前 viewport
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);

            // 渲染到 cubemap 的 6 个面
            fbo.Bind();
            glViewport(0, 0, config.face_size, config.face_size);
            config.shader.Use();

            for (int i = 0; i < 6; ++i)
            {
                fbo.AttachCubemapFace(config.target_cubemap.GetID(), i, GL_COLOR_ATTACHMENT0);

                CameraUBOData matrices;
                matrices.projection = projection;
                matrices.view = kCubemapViews[i];
                internal_ubo.SetData(&matrices, sizeof(matrices));

                // 调用渲染前回调（如果有）
                if (config.on_before_face_render)
                {
                    config.on_before_face_render(config.shader, i);
                }

                glClear(GL_COLOR_BUFFER_BIT);
                cube.Draw();
            }

            fbo.Unbind();
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        }

    } // anonymous namespace

    // ========== TextureCubemap 实现 ==========

    TextureCubemap::TextureCubemap(int width, int height,
                                   const std::array<const void *, 6> &face_data,
                                   const TextureSpecification &spec)
        : handle_(GL_TEXTURE_CUBE_MAP), spec_(spec), size_(width)
    {
        ValidateSize(width);

        if (width != height)
        {
            throw TextureException("Cubemap faces must be square");
        }

        handle_.Bind();

        GLenum internal_format = static_cast<GLenum>(spec_.internal_format);
        GLenum format = static_cast<GLenum>(spec_.format);
        GLenum type = static_cast<GLenum>(spec_.data_type);

        // 上传 6 个面的数据
        for (uint32_t i = 0; i < 6; ++i)
        {
            GLenum target = GetCubemapFaceTarget(i);
            glTexImage2D(target, 0, internal_format, width, height, 0, format, type, face_data[i]);
        }

        // 应用参数
        ApplyParameters();

        // 生成 mipmap
        if (spec_.generate_mipmaps)
        {
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        }

        handle_.Unbind();
    }

    TextureCubemap::TextureCubemap(int size, const TextureSpecification &spec)
        : handle_(GL_TEXTURE_CUBE_MAP), spec_(spec), size_(size)
    {
        ValidateSize(size);

        handle_.Bind();

        GLenum internal_format = static_cast<GLenum>(spec_.internal_format);
        GLenum format = static_cast<GLenum>(spec_.format);
        GLenum type = static_cast<GLenum>(spec_.data_type);

        // 创建空的 cubemap（6 个面）
        for (uint32_t i = 0; i < 6; ++i)
        {
            GLenum target = GetCubemapFaceTarget(i);
            glTexImage2D(target, 0, internal_format, size, size, 0, format, type, nullptr);
        }

        // 应用参数
        ApplyParameters();

        handle_.Unbind();
    }

    TextureCubemap TextureCubemap::LoadFromFiles(
        const std::array<std::string, 6> &paths,
        const TextureSpecification &spec)
    {
        std::array<TextureData, 6> face_data;
        int size = 0;

        // 加载所有面
        for (uint32_t i = 0; i < 6; ++i)
        {
            bool is_hdr = (paths[i].find(".hdr") != std::string::npos ||
                           paths[i].find(".exr") != std::string::npos);

            face_data[i] = is_hdr ? TextureLoader::LoadHDRFromFile(paths[i], false)
                                  : TextureLoader::LoadFromFile(paths[i], false); // cubemap 通常不翻转

            if (!face_data[i].IsValid())
            {
                throw TextureException("Failed to load cubemap face: " + paths[i]);
            }

            // 验证尺寸
            if (face_data[i].width != face_data[i].height)
            {
                throw TextureException("Cubemap face must be square: " + paths[i]);
            }

            if (i == 0)
            {
                size = face_data[i].width;
            }
            else if (face_data[i].width != size || face_data[i].height != size)
            {
                throw TextureException("All cubemap faces must have the same size. Face " +
                                       std::to_string(i) + " size mismatch: " + paths[i]);
            }
        }

        // 确定格式
        TextureSpecification final_spec = spec;
        int channels = face_data[0].channels;
        bool is_hdr = face_data[0].is_hdr;

        if (!is_hdr)
        {
            switch (channels)
            {
            case 1:
                final_spec.format = TextureFormat::Red;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::R8;
                break;
            case 2:
                final_spec.format = TextureFormat::RG;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::RG8;
                break;
            case 3:
                final_spec.format = TextureFormat::RGB;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::RGB8;
                break;
            case 4:
                final_spec.format = TextureFormat::RGBA;
                if (spec.internal_format == TextureSpecification{}.internal_format)
                    final_spec.internal_format = TextureInternalFormat::RGBA8;
                break;
            }
        }
        else
        {
            final_spec.data_type = TextureDataType::Float;
            switch (channels)
            {
            case 1:
                final_spec.format = TextureFormat::Red;
                final_spec.internal_format = TextureInternalFormat::R32F;
                break;
            case 2:
                final_spec.format = TextureFormat::RG;
                final_spec.internal_format = TextureInternalFormat::RG32F;
                break;
            case 3:
                final_spec.format = TextureFormat::RGB;
                final_spec.internal_format = TextureInternalFormat::RGB32F;
                break;
            case 4:
                final_spec.format = TextureFormat::RGBA;
                final_spec.internal_format = TextureInternalFormat::RGBA32F;
                break;
            }
        }

        // 准备数据指针数组
        std::array<const void *, 6> data_ptrs;
        for (int i = 0; i < 6; ++i)
        {
            data_ptrs[i] = face_data[i].pixels.get();
        }

        return TextureCubemap(size, size, data_ptrs, final_spec);
    }

    TextureCubemap TextureCubemap::LoadFromEquirectangular(
        const std::string &path,
        int cubemap_size,
        const TextureSpecification &spec)
    {
        // 加载等距矩形 HDR 纹理
        auto equirect_tex = Texture2D::LoadFromFile(path, texture_presets::HDREquirect());
        if (!equirect_tex.IsValid())
        {
            throw TextureException("Failed to load equirectangular texture: " + path);
        }

        // Equirectangular 转换专用的片段着色器
        const char *fs_source = R"(
            #version 460 core
            in vec3 vs_posOS;
            out vec4 o_Color;
            layout(binding = 0) uniform sampler2D u_EquirectMap;
            
            vec2 SampleSphericalMap(vec3 v) {
                vec3 dir = normalize(v);
                vec2 uv = vec2(atan(dir.z, dir.x), asin(dir.y));
                uv *= vec2(0.1591, 0.3183);  // 1/(2*PI), 1/PI
                uv += 0.5;
                return uv;
            }
            
            void main() {
                vec3 hdrColor = texture(u_EquirectMap, SampleSphericalMap(vs_posOS)).rgb;
                o_Color = vec4(hdrColor, 1.0);
            }
        )";

        Shader conversion_shader = Shader::CreateFromSource(kCubemapVS, fs_source);

        // 创建目标 cubemap
        TextureCubemap cubemap(cubemap_size, spec);

        // 绑定输入纹理
        Shader::BindTextureToUnit(equirect_tex.GetID(), 0, GL_TEXTURE_2D);

        // 使用公共辅助函数渲染
        RenderToCubemapConfig config{cubemap, conversion_shader, cubemap_size, nullptr};
        RenderToCubemapFaces(config);

        // 生成 mipmap（如果需要）
        if (spec.generate_mipmaps)
        {
            cubemap.GenerateMipmaps();
        }

        return cubemap;
    }

    TextureCubemap TextureCubemap::ConvolveIrradiance(
        const TextureCubemap &source_cubemap,
        int irradiance_size,
        int sample_count,
        const TextureSpecification &spec)
    {
        if (!source_cubemap.IsValid())
        {
            throw TextureException("ConvolveIrradiance: source cubemap is invalid");
        }

        // Irradiance 卷积专用的片段着色器（蒙特卡洛采样）
        const char *fs_source = R"(
            #version 460 core
            in vec3 vs_posOS;
            out vec4 o_Color;
            layout(binding = 0) uniform samplerCube u_EnvCubemap;
            layout(location = 0) uniform int u_SampleCount;

            const float PI = 3.14159265359;
            const float PiOver4 = 0.7853981633974483;
            const float PiOver2 = 1.5707963267948966;

            // PCG hash
            uint Hash(uint x) {
                x ^= x >> 16;
                x *= 0x7feb352du;
                x ^= x >> 15;
                x *= 0x846ca68bu;
                x ^= x >> 16;
                return x;
            }

            float UintTo01(uint x) {
                return float(x & 0x00ffffffu) / float(0x01000000u);
            }

            vec2 Rand2(uvec2 pixel, int sampleIdx) {
                uint s0 = Hash(pixel.x ^ (pixel.y * 1664525u) ^ uint(sampleIdx));
                uint s1 = Hash(s0 + 0x9e3779b9u);
                return vec2(UintTo01(s0), UintTo01(s1));
            }

            // Frisvad's orthonormal basis
            void GetONB(vec3 n, out vec3 b1, out vec3 b2) {
                float s = mix(-1.0, 1.0, step(0.0, n.z));
                float a = -1.0 / (s + n.z);
                float b = n.x * n.y * a;
                b1 = vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
                b2 = vec3(b, s + n.y * n.y * a, -n.y);
            }

            vec2 SampleUniformDiskConcentric(vec2 u) {
                vec2 uOffset = 2.0 * u - vec2(1.0);
                if (uOffset.x == 0.0 && uOffset.y == 0.0)
                    return vec2(0.0);
                float theta, r;
                if (abs(uOffset.x) > abs(uOffset.y)) {
                    r = uOffset.x;
                    theta = PiOver4 * (uOffset.y / uOffset.x);
                } else {
                    r = uOffset.y;
                    theta = PiOver2 - PiOver4 * (uOffset.x / uOffset.y);
                }
                return r * vec2(cos(theta), sin(theta));
            }

            vec3 SampleCosineHemisphere(vec2 u) {
                vec2 d = SampleUniformDiskConcentric(u);
                float z = sqrt(max(0.0, 1.0 - d.x * d.x - d.y * d.y));
                return vec3(d.x, d.y, z);
            }

            void main() {
                uvec2 pix = uvec2(gl_FragCoord.xy);
                vec3 N = normalize(vs_posOS);
                vec3 B1, B2;
                GetONB(N, B1, B2);

                vec3 irradiance = vec3(0.0);
                for (int i = 0; i < u_SampleCount; i++) {
                    vec2 u = Rand2(pix, i);
                    vec3 L = SampleCosineHemisphere(u);
                    L = normalize(L.x * B1 + L.y * B2 + L.z * N);
                    irradiance += texture(u_EnvCubemap, L).rgb;
                }

                o_Color = vec4(PI * irradiance / float(u_SampleCount), 1.0);
            }
        )";

        Shader irradiance_shader = Shader::CreateFromSource(kCubemapVS, fs_source);

        // 创建目标 irradiance cubemap
        TextureCubemap irradiance_cubemap(irradiance_size, spec);

        // 绑定源 cubemap
        Shader::BindTextureToUnit(source_cubemap.GetID(), 0, GL_TEXTURE_CUBE_MAP);

        // 使用公共辅助函数渲染
        RenderToCubemapConfig config{
            irradiance_cubemap,
            irradiance_shader,
            irradiance_size,
            [sample_count](Shader & /*shader*/, int /*face*/)
            {
                glUniform1i(0, sample_count); // location 0 = u_SampleCount
            }};
        RenderToCubemapFaces(config);

        // 生成 mipmap（如果需要）
        if (spec.generate_mipmaps)
        {
            irradiance_cubemap.GenerateMipmaps();
        }

        return irradiance_cubemap;
    }

    void TextureCubemap::Bind(uint32_t slot) const noexcept
    {
        if (handle_.IsValid())
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            handle_.Bind();
        }
    }

    void TextureCubemap::Unbind() const noexcept
    {
        handle_.Unbind();
    }

    void TextureCubemap::SetFaceData(uint32_t face_index, const void *data, int width, int height)
    {
        if (face_index >= 6)
        {
            throw TextureException("SetFaceData: invalid face index " + std::to_string(face_index));
        }
        if (!data)
        {
            throw TextureException("SetFaceData: data pointer is null");
        }
        if (width != size_ || height != size_)
        {
            throw TextureException("SetFaceData: face size mismatch");
        }

        handle_.Bind();
        GLenum target = GetCubemapFaceTarget(face_index);
        glTexSubImage2D(target, 0, 0, 0, width, height,
                        static_cast<GLenum>(spec_.format),
                        static_cast<GLenum>(spec_.data_type),
                        data);
        handle_.Unbind();
    }

    void TextureCubemap::GenerateMipmaps() noexcept
    {
        if (handle_.IsValid())
        {
            handle_.Bind();
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            handle_.Unbind();
        }
    }

    void TextureCubemap::SetWrapMode(TextureWrap wrap_s, TextureWrap wrap_t, TextureWrap wrap_r)
    {
        spec_.wrap_s = wrap_s;
        spec_.wrap_t = wrap_t;
        spec_.wrap_r = wrap_r;

        handle_.Bind();
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap_s));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap_t));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, static_cast<GLint>(wrap_r));
        handle_.Unbind();
    }

    void TextureCubemap::SetFilterMode(TextureFilter min_filter, TextureFilter mag_filter)
    {
        spec_.min_filter = min_filter;
        spec_.mag_filter = mag_filter;

        handle_.Bind();
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(min_filter));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(mag_filter));
        handle_.Unbind();
    }

    void TextureCubemap::ApplyParameters()
    {
        // 环绕模式
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, static_cast<GLint>(spec_.wrap_s));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, static_cast<GLint>(spec_.wrap_t));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, static_cast<GLint>(spec_.wrap_r));

        // 过滤模式
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(spec_.min_filter));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(spec_.mag_filter));

        // Cubemap 默认使用 ClampToEdge 防止接缝
        if (spec_.wrap_s == TextureWrap::Repeat || spec_.wrap_t == TextureWrap::Repeat || spec_.wrap_r == TextureWrap::Repeat)
        {
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
    }

    void TextureCubemap::ValidateSize(int size) const
    {
        if (size <= 0)
        {
            throw TextureException("Invalid cubemap size: " + std::to_string(size));
        }

        // 检查是否超过 OpenGL 限制
        GLint max_size = 0;
        glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max_size);

        if (size > max_size)
        {
            throw TextureException("Cubemap size exceeds maximum: " + std::to_string(max_size));
        }
    }

    GLenum TextureCubemap::GetCubemapFaceTarget(uint32_t face_index) noexcept
    {
        static const GLenum targets[6] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_X, // Right
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X, // Left
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y, // Top
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, // Bottom
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z, // Front
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z  // Back
        };

        return (face_index < 6) ? targets[face_index] : GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

} // namespace zk_pbr::gfx

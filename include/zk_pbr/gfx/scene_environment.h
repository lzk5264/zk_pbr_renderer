#pragma once
#include <string>
#include <zk_pbr/gfx/texture_cubemap.h>
#include <zk_pbr/gfx/texture2d.h>

namespace zk_pbr::gfx
{
    // IBL 预计算参数配置
    struct IBLConfig
    {
        int env_cubemap_size = 1024;
        int irradiance_size = 32; // 低频分量，32² 足应于实时渲染
        int irradiance_samples = 512;
        int prefiltered_size = 256; // 512 及以上的收益递减，缓存行为变差
        int prefiltered_samples = 1024;
        int dfg_lut_size = 512; // 业界标准分辨率，改这个会很倒霉
        int dfg_lut_samples = 1024;
    };

    // 场景级环境光照。从 HDR 贴图预计算 IBL 资源。
    //
    // 该类管理三张关键贴图：
    //   1. irradiance_   - 漫反射项（低频，移动设备的瓶颈）
    //   2. prefiltered_  - 镜面反射项（多粗糙度等级的 mipmap）
    //   3. dfg_lut_      - 菲涅尔项查询表（split-sum approximation 的必需品）
    //
    // 使用场景：
    //   - 场景初始化时调用 LoadFromHDR 一次
    //   - 每帧渲染时调用 BindIBL 一次（随后 shader 采样这些纹理）
    class SceneEnvironment
    {
    public:
        // 为什么是静态工厂而非构造函数：
        //   - 异常处理，IBL资源加载与计算有一定先后顺序
        // 后续可以实现从其他形式资源导入的类似函数

        /// @brief 从等距矩形 HDR 构建完整的 IBL 资源集
        /// @param hdr_path 等距矩形贴图路径.
        /// @param config   IBL 预计算参数配置，一般默认.
        /// @return SceneEnviroment 对象
        static SceneEnvironment LoadFromHDR(const std::string &hdr_path,
                                            const IBLConfig &config = IBLConfig{});

        // 将 IBL 纹理绑定到当前 GL context 的指定纹理单元。
        //
        // 绑定对应关系（硬编码，改这些值会破坏 pbr_shading_fs.frag 中的采样器）：
        //   - GL_TEXTURE5: irradiance_  (shader: uniform sampler2D sampler_irradiance)
        //   - GL_TEXTURE6: prefiltered_ (shader: uniform samplerCube sampler_prefiltered_env)
        //   - GL_TEXTURE7: dfg_lut_     (shader: uniform sampler2D sampler_dfg_lut)
        //
        // 注意事项：
        //   - env_cubemap 未绑定，由 skybox pass 自行管理
        void BindIBL() const;

        const TextureCubemap &GetEnvCubemap() const { return env_cubemap_; }
        const TextureCubemap &GetIrradianceMap() const { return irradiance_; }
        const TextureCubemap &GetPrefilteredEnv() const { return prefiltered_; }
        const Texture2D &GetDFGLUT() const { return dfg_lut_; }

    private:
        TextureCubemap env_cubemap_;
        TextureCubemap irradiance_;
        TextureCubemap prefiltered_;
        Texture2D dfg_lut_;
    };
}
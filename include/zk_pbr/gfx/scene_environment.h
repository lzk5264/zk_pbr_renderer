#pragma once
#include <string>
#include <zk_pbr/gfx/texture_cubemap.h>
#include <zk_pbr/gfx/texture2d.h>

namespace zk_pbr::gfx
{
    // IBL 预计算资源的分辨率与采样数配置
    // 各参数与屏幕分辨率无关，取决于 HDR 源文件质量与性能预算
    struct IBLConfig
    {
        int env_cubemap_size = 1024; // 从等距矩形转换的环境 Cubemap 边长
        int irradiance_size = 32;    // 漫反射 Irradiance Map 边长（低频信号，32 足够）
        int irradiance_samples = 512;
        int prefiltered_size = 256; // 预滤环境贴图边长
        int prefiltered_samples = 1024;
        int dfg_lut_size = 512; // DFG 查找表边长
        int dfg_lut_samples = 1024;
    };

    // 场景级 IBL 环境资源管理
    class SceneEnvironment
    {
    public:
        // 从一张等距矩形 HDR 贴图构建完整 IBL 资源链
        static SceneEnvironment LoadFromHDR(const std::string &hdr_path,
                                            const IBLConfig &config = IBLConfig{});
        // 绑定 IBL 所需的三张贴图到 slot 5/6/7（供 PBR shading pass 使用）
        // env_cubemap 由天空盒 pass 自行绑定，此处不管理
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
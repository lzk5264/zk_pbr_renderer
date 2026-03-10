#include <zk_pbr/gfx/scene_environment.h>
#include <zk_pbr/gfx/render_constants.h>

namespace zk_pbr::gfx
{

    SceneEnvironment SceneEnvironment::LoadFromHDR(const std::string &hdr_path,
                                                   const IBLConfig &config)
    {
        SceneEnvironment se;

        se.env_cubemap_ = TextureCubemap::LoadFromEquirectangular(
            hdr_path,
            config.env_cubemap_size,
            TexturePresets::HDRCubemap());

        se.irradiance_ = TextureCubemap::ConvolveIrradiance(
            se.env_cubemap_,
            config.irradiance_size,
            config.irradiance_samples,
            TexturePresets::IrradianceMap());

        se.prefiltered_ = TextureCubemap::PrefilteredEnvMap(
            se.env_cubemap_,
            config.prefiltered_size,
            config.prefiltered_samples,
            TexturePresets::PrefilteredEnvMap());

        se.dfg_lut_ = Texture2D::ComputeDFG(
            config.dfg_lut_size,
            config.dfg_lut_samples,
            false,
            TexturePresets::DFGLUT());

        return se;
    }
    void SceneEnvironment::BindIBL() const
    {
        irradiance_.Bind(texture_slot::kIrradianceMap);
        prefiltered_.Bind(texture_slot::kPrefilteredEnvMap);
        dfg_lut_.Bind(texture_slot::kDFGLUT);
    }
}
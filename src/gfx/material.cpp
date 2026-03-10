#include <zk_pbr/gfx/material.h>
#include <zk_pbr/gfx/render_constants.h>
#include <glm/gtc/type_ptr.hpp>

namespace zk_pbr::gfx
{

    const std::shared_ptr<Texture2D> &DefaultTextures::White()
    {
        static auto tex = std::make_shared<Texture2D>(
            Texture2D::CreateSolid(255, 255, 255));
        return tex;
    }

    const std::shared_ptr<Texture2D> &DefaultTextures::FlatNormal()
    {
        static auto tex = std::make_shared<Texture2D>(
            Texture2D::CreateSolid(128, 128, 255));
        return tex;
    }
    const std::shared_ptr<Texture2D> &DefaultTextures::Black()
    {
        static auto tex = std::make_shared<Texture2D>(
            Texture2D::CreateSolid(0, 0, 0));
        return tex;
    }
    const std::shared_ptr<Texture2D> &DefaultTextures::DefaultMR()
    {
        static auto tex = std::make_shared<Texture2D>(
            Texture2D::CreateSolid(0, 255, 0));
        return tex;
    }
    void Material::Bind() const
    {
        // 纹理绑定：空指针回退到对应的默认纹理
        (albedo ? albedo : DefaultTextures::White())->Bind(texture_slot::kAlbedo);
        (normal ? normal : DefaultTextures::FlatNormal())->Bind(texture_slot::kNormal);
        (metallic_roughness ? metallic_roughness : DefaultTextures::DefaultMR())->Bind(texture_slot::kMetallicRoughness);
        (ao ? ao : DefaultTextures::White())->Bind(texture_slot::kAO);
        (emissive ? emissive : DefaultTextures::Black())->Bind(texture_slot::kEmissive);

        // 因子绑定：explicit location
        glUniform4fv(uniform_location::kBaseColorFactor, 1, glm::value_ptr(base_color_factor));
        glUniform1f(uniform_location::kMetallicFactor, metallic_factor);
        glUniform1f(uniform_location::kRoughnessFactor, roughness_factor);
        glUniform3fv(uniform_location::kEmissiveFactor, 1, glm::value_ptr(emissive_factor));
    }
}

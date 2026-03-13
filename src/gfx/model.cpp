#include <iostream>
#define CGLTF_IMPLEMENTATION
#include <third_party/cgltf/cgltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <zk_pbr/gfx/model.h>

namespace zk_pbr::gfx
{

    namespace
    {
        // 内部辅助函数，被 TraverseNode 函数调用，给定 cgltf_primitive
        // 提取primitive内部数据转换为渲染器内部mesh格式并返回
        Mesh ExtraMesh(const cgltf_primitive primitive)
        {
            // 判断是否是索引绘制方式
            const cgltf_accessor *indices_acc = primitive.indices;
            std::vector<unsigned int> indices;
            if (indices_acc != nullptr)
            {
                indices.resize(indices_acc->count);
                cgltf_accessor_unpack_indices(indices_acc, indices.data(), sizeof(unsigned int), indices_acc->count);
            }

            const cgltf_accessor *pos_acc = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
            const cgltf_accessor *nor_acc = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
            const cgltf_accessor *uv_acc = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor *tan_acc = cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0);

            // POSITION 是唯一不可缺少的
            if (!pos_acc)
                throw ModelException("primitive missing POSITION");

            cgltf_size vertex_count = pos_acc->count;
            std::vector<float> vertices;
            // 按照 vertex_layout.cpp ，PBR layout一个顶点是12个float
            vertices.reserve(12 * vertex_count);
            for (int j = 0; j < vertex_count; j++)
            {
                float pos[3];
                cgltf_accessor_read_float(pos_acc, j, pos, 3);
                vertices.insert(vertices.end(), pos, pos + 3);

                // NORMAL（没有则填默认值）
                float nor[3] = {0.f, 1.f, 0.f};
                if (nor_acc)
                    cgltf_accessor_read_float(nor_acc, j, nor, 3);
                vertices.insert(vertices.end(), nor, nor + 3);

                // TEXCOORD_0（没有则填 0,0）
                float uv[2] = {0.f, 0.f};
                if (uv_acc)
                    cgltf_accessor_read_float(uv_acc, j, uv, 2);
                vertices.insert(vertices.end(), uv, uv + 2);

                // TANGENT（没有则填默认值）
                float tan[4] = {1.f, 0.f, 0.f, 1.f};
                if (tan_acc)
                    cgltf_accessor_read_float(tan_acc, j, tan, 4);
                vertices.insert(vertices.end(), tan, tan + 4);
            }

            return Mesh(vertices.data(), vertex_count, indices.data(), indices.size(), layouts::PBRVertex());
        }

        // 内部辅助函数，被 ExtraMaterial 调用，
        // 用于解析 cgltf_sampler 对象生成渲染器内部的 TextureSPec 格式
        // 基于被解析纹理的属性（存储的是否是颜色），决定用 srgb 处理
        TextureSpec ParseGltfSampler(const cgltf_sampler *sampler, bool is_srgb)
        {
            TextureSpec spec = is_srgb ? TexturePresets::LDRColorMap() : TexturePresets::LDRDataMap();

            if (!sampler)
                return spec;

            // glTF 有关 wrap 与 filter 的设计标准，与 GL 的枚举值相同
            // 而该渲染器 TextureWarp 中的枚举值也是这么做的，所以两者可以直接转换

            // 简化设计， 可能wrap_s != wrap_t，不过目前我们的系统中，先共用
            spec.wrap = static_cast<TextureWrap>(sampler->wrap_s);

            // filter mode
            if (sampler->mag_filter != 0)
                spec.mag_filter = static_cast<TextureFilter>(sampler->mag_filter);
            if (sampler->min_filter != 0)
            {
                spec.min_filter = static_cast<TextureFilter>(sampler->min_filter);
                // 是否包含 Mipmap 标志可以根据 min_filter 推断
                if (sampler->min_filter == 9728 || sampler->min_filter == 9729)
                {
                    spec.generate_mipmaps = false; // GL_NEAREST 或 GL_LINEAR (无mipmap)
                }
                else
                {
                    spec.generate_mipmaps = true; // GL_LINEAR_MIPMAP_LINEAR 等
                }
            }
            return spec;
        }

        Material ExtraMaterial(const cgltf_primitive &primitive)
        {
            Material material;
            const cgltf_material *cgltf_material = primitive.material;
            // 直接返回兜底 material
            if (!cgltf_material || !cgltf_material->has_pbr_metallic_roughness)
            {
                return material;
            }

            const cgltf_pbr_metallic_roughness &cgltf_MR = cgltf_material->pbr_metallic_roughness;

            material.base_color_factor = glm::make_vec4(cgltf_MR.base_color_factor);
            material.metallic_factor = cgltf_MR.metallic_factor;
            material.roughness_factor = cgltf_MR.roughness_factor;
            material.emissive_factor = glm::make_vec3(cgltf_material->emissive_factor);

            return material;
        }
    }

    void Model::TraverseNode(const cgltf_node *node, Model &model, glm::mat4 world_matrix)
    {
        cgltf_float local_transform_matrix[16];
        cgltf_node_transform_local(node, local_transform_matrix);
        glm::mat4 local_matrix = glm::make_mat4(local_transform_matrix);

        world_matrix = world_matrix * local_matrix;

        if (node->mesh)
        {
            for (int i = 0; i < node->mesh->primitives_count; i++)
            {
                const cgltf_primitive &primitive = node->mesh->primitives[i];

                Material material;

                model.primitives_.push_back({ExtraMesh(primitive), material, world_matrix});
            }
        }

        for (int i = 0; i < node->children_count; i++)
        {
            TraverseNode(node->children[i], model, world_matrix);
        }
    }

    Model Model::LoadFromGLTF(const std::string &path)
    {
        Model model;

        // 代码参考自 cgltf github README.MD 以及 cgltf.h 注释
        cgltf_options options = {};
        cgltf_data *data = nullptr;
        // 确定数据格式与布局
        cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
        if (result != cgltf_result_success)
        {
            throw std::runtime_error("Failed to parse glTF file: " + path);
        }
        // 导入底层数据
        result = cgltf_load_buffers(&options, data, path.c_str());
        if (result != cgltf_result_success)
        {
            cgltf_free(data);
            throw std::runtime_error("Failed to load glTF buffers: " + path);
        }
        // 验证
        result = cgltf_validate(data);
        if (result != cgltf_result_success)
        {
            cgltf_free(data);
            throw std::runtime_error("Invalid glTF file: " + path);
        }

        // 先处理单场景但模型的情况，后续扩展
        const cgltf_scene *scene = data->scene;
        if (scene)
        {
            for (cgltf_size i = 0; i < scene->nodes_count; i++)
            {
                TraverseNode(scene->nodes[i], model, glm::mat4{1.0f});
            }
        }

        cgltf_free(data);

        return model;
    }

    void Model::Draw(const Shader &shader) const
    {
        for (const auto &primitive : primitives_)
        {
            shader.SetMat4("u_Model", primitive.model_matrix);
            primitive.material.Bind();
            primitive.mesh.Draw();
        }
    }
} // namespace zk_pbr::gfx
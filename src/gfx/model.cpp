#include <iostream>
#include <filesystem>
#include <memory>
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <zk_pbr/gfx/model.h>

namespace zk_pbr::gfx
{

    namespace
    {
        // 内部辅助函数，被 TraverseNode 函数调用，给定 cgltf_primitive
        // 提取primitive内部数据转换为渲染器内部mesh格式并返回
        Mesh ExtractMesh(const cgltf_primitive &primitive)
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

            constexpr cgltf_size kFloatsPerVertex = 12;
            cgltf_size vertex_count = pos_acc->count;
            std::vector<float> vertices(kFloatsPerVertex * vertex_count);
            float *dst = vertices.data();
            for (cgltf_size j = 0; j < vertex_count; j++, dst += kFloatsPerVertex)
            {
                cgltf_accessor_read_float(pos_acc, j, dst, 3);

                // NORMAL（没有则填默认值）
                if (nor_acc)
                    cgltf_accessor_read_float(nor_acc, j, dst + 3, 3);
                else
                {
                    dst[3] = 0.f;
                    dst[4] = 1.f;
                    dst[5] = 0.f;
                }

                // TEXCOORD_0（没有则填 0,0）
                if (uv_acc)
                    cgltf_accessor_read_float(uv_acc, j, dst + 6, 2);
                else
                {
                    dst[6] = 0.f;
                    dst[7] = 0.f;
                }

                // TANGENT（没有则填默认值）
                if (tan_acc)
                    cgltf_accessor_read_float(tan_acc, j, dst + 8, 4);
                else
                {
                    dst[8] = 1.f;
                    dst[9] = 0.f;
                    dst[10] = 0.f;
                    dst[11] = 1.f;
                }
            }

            return Mesh(vertices.data(), vertex_count, indices.data(), indices.size(), layouts::PBRVertex());
        }

        // 内部辅助函数，被 ParseGltfTextureView 调用，
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
            // TODO: 扩展渲染器系统，同时存储 wrap_s 与 wrap_t
            spec.wrap = static_cast<TextureWrap>(sampler->wrap_s);

            // filter mode
            if (sampler->mag_filter != 0)
                spec.mag_filter = static_cast<TextureFilter>(sampler->mag_filter);
            if (sampler->min_filter != 0)
            {
                spec.min_filter = static_cast<TextureFilter>(sampler->min_filter);
                // 是否包含 Mipmap 标志可以根据 min_filter 推断
                if (sampler->min_filter == GL_NEAREST || sampler->min_filter == GL_LINEAR)
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

        int HexCharToInt(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }

        // 简单的 uri-decode 实现，用于处理 glTF 纹理路径中的百分号编码（如 %20 -> 空格）
        std::string UriDecode(const std::string &src)
        {
            std::string result;
            result.reserve(src.length());
            for (size_t i = 0; i < src.length(); ++i)
            {
                if (src[i] == '%' && i + 2 < src.length())
                {
                    int hi = HexCharToInt(src[i + 1]);
                    int lo = HexCharToInt(src[i + 2]);
                    if (hi >= 0 && lo >= 0)
                    {
                        result += static_cast<char>((hi << 4) | lo);
                        i += 2;
                    }
                    else
                    {
                        result += '%';
                    }
                }
                else
                {
                    result += src[i];
                }
            }
            return result;
        }

        // 内部辅助函数，被 ParseGltfTextureView 调用
        // 给定 cgltf_texture_view，获取 cgltf_image 对象存储的 url 路径
        // 与 base_path 结合成纹理图像的绝对路径，该函数适用于gltf格式的纹理加载
        // 如果没有路径则返回空
        std::string GetGltfTexture2DPath(const cgltf_texture_view &view, const std::string &base_path)
        {
            if (!view.texture || !view.texture->image)
            {
                return "";
            }

            const cgltf_image *image = view.texture->image;

            if (image->uri)
            {
                // 做 uri-decode 以兼容包含空格或特殊字符的文件名
                std::string decoded_uri = UriDecode(image->uri);
                std::filesystem::path full_path = std::filesystem::path(base_path) / decoded_uri;
                return full_path.generic_string();
            }

            return "";
        }

        // 内部辅助函数，被 ExtractMaterial 调用
        // 调用GetGltfTexture2DPath 与 ParseGltfSampler 后，直接调用 Texture2D::LoadFromFile
        // 若无纹理数据或发生加载异常，则静默拦截异常，输出警告并返回 nullptr，将兜底责任交还给 Material。
        std::shared_ptr<Texture2D> ParseGltfTextureView(
            const cgltf_texture_view &view,
            const std::string &base_path,
            bool is_srgb)
        {
            std::string path = GetGltfTexture2DPath(view, base_path);
            if (path.empty())
            {
                return nullptr;
            }

            TextureSpec spec = ParseGltfSampler(view.texture->sampler, is_srgb);

            try
            {
                // [进阶预留]: 这里未来应该先去一个 std::unordered_map<string, shared_ptr<Texture2D>> texture_cache_ 中查找 path
                // 如果命中 cache，直接 return cache[path]

                // glTF 纹理不需要翻转（UV 原点已在左上角，符合 OpenGL 约定）
                return std::make_shared<Texture2D>(Texture2D::LoadFromFile(path, spec, false));
            }
            catch (const std::exception &e)
            {
                // 只输出警告，不阻断模型加载。返回 nullptr 将触发 material 里的 DefaultTextures 回退
                std::cerr << "[Warning] Failed to load texture: " << path << " | Reason: " << e.what() << "\n";
                return nullptr;
            }
        }

        Material ExtractMaterial(const cgltf_primitive &primitive, const std::string &base_path)
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

            material.albedo = ParseGltfTextureView(cgltf_MR.base_color_texture, base_path, true);
            material.metallic_roughness = ParseGltfTextureView(cgltf_MR.metallic_roughness_texture, base_path, false);
            material.emissive = ParseGltfTextureView(cgltf_material->emissive_texture, base_path, true);
            material.ao = ParseGltfTextureView(cgltf_material->occlusion_texture, base_path, false);
            material.normal = ParseGltfTextureView(cgltf_material->normal_texture, base_path, false);
            return material;
        }
    } // namespace

    void Model::TraverseNode(const cgltf_node *node, Model &model, glm::mat4 world_matrix, const std::string &base_path)
    {

        // 获取子变换矩阵，与父变换矩阵结合为世界变换矩阵
        cgltf_float local_transform_matrix[16];
        cgltf_node_transform_local(node, local_transform_matrix);
        glm::mat4 local_matrix = glm::make_mat4(local_transform_matrix);

        world_matrix = world_matrix * local_matrix;

        // node 可以不包含任何 mesh 信息
        if (node->mesh)
        {
            for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
            {
                const cgltf_primitive &primitive = node->mesh->primitives[i];

                model.primitives_.push_back({ExtractMesh(primitive), ExtractMaterial(primitive, base_path), world_matrix});
            }
        }

        // 递归处理子节点
        for (cgltf_size i = 0; i < node->children_count; i++)
        {
            TraverseNode(node->children[i], model, world_matrix, base_path);
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
            throw ModelException("Failed to parse glTF file: " + path);
        }

        // RAII guard: 保证 cgltf_data 在任何退出路径（含异常）下都被释放
        auto data_guard = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>(data, &cgltf_free);

        // 导入底层数据
        result = cgltf_load_buffers(&options, data, path.c_str());
        if (result != cgltf_result_success)
        {
            throw ModelException("Failed to load glTF buffers: " + path);
        }
        // 验证
        result = cgltf_validate(data);
        if (result != cgltf_result_success)
        {
            throw ModelException("Invalid glTF file: " + path);
        }

        // 父目录
        std::string base_dir = std::filesystem::path(path).parent_path().string();

        // 先处理单场景但模型的情况，后续扩展
        const cgltf_scene *scene = data->scene;
        if (scene)
        {
            for (cgltf_size i = 0; i < scene->nodes_count; i++)
            {
                TraverseNode(scene->nodes[i], model, glm::mat4{1.0f}, base_dir);
            }
        }

        return model;
    }

    std::vector<DrawCommand> Model::GetDrawCommands() const
    {
        std::vector<DrawCommand> commands;
        commands.reserve(primitives_.size());
        for (const auto &prim : primitives_)
        {
            glm::mat4 model_inv_t = glm::transpose(glm::inverse(prim.model_matrix));
            commands.push_back({&prim.mesh, &prim.material, prim.model_matrix, model_inv_t});
        }
        return commands;
    }
} // namespace zk_pbr::gfx

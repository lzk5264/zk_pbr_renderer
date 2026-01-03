#include <zk_pbr/gfx/geometry_utils.h>

#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace zk_pbr::gfx::GeometryUtils
{

    // 顶点结构（用于临时存储）
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec3 tangent;
    };

    Mesh CreateSphere(float radius, int segments)
    {
        if (segments < 3)
        {
            throw MeshException("Sphere segments must be >= 3");
        }

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // 生成顶点
        for (int lat = 0; lat <= segments; ++lat)
        {
            float theta = lat * glm::pi<float>() / segments; // 纬度角 [0, π]
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            for (int lon = 0; lon <= segments; ++lon)
            {
                float phi = lon * 2.0f * glm::pi<float>() / segments; // 经度角 [0, 2π]
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                Vertex vertex;

                // 位置：球面坐标转笛卡尔坐标
                vertex.position.x = radius * sinTheta * cosPhi;
                vertex.position.y = radius * cosTheta;
                vertex.position.z = radius * sinTheta * sinPhi;

                // 法线：单位球面法线 = 归一化位置向量
                vertex.normal = glm::normalize(vertex.position);

                // UV：经纬度映射到 [0, 1]
                vertex.uv.x = static_cast<float>(lon) / segments;
                vertex.uv.y = static_cast<float>(lat) / segments;

                // 切线：沿经线方向（东方向）
                vertex.tangent.x = -sinPhi;
                vertex.tangent.y = 0.0f;
                vertex.tangent.z = cosPhi;
                vertex.tangent = glm::normalize(vertex.tangent);

                vertices.push_back(vertex);
            }
        }

        // 生成索引（四边形分解为两个三角形）
        for (int lat = 0; lat < segments; ++lat)
        {
            for (int lon = 0; lon < segments; ++lon)
            {
                int first = lat * (segments + 1) + lon;
                int second = first + segments + 1;

                // 第一个三角形
                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                // 第二个三角形
                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        // 构建顶点布局：position(vec3), normal(vec3), uv(vec2), tangent(vec3)
        // 总步长：11 * sizeof(float)
        constexpr size_t stride = 11 * sizeof(float);
        VertexLayout layout;
        layout.AddFloat(0, 3, stride, 0);                 // position
        layout.AddFloat(1, 3, stride, 3 * sizeof(float)); // normal
        layout.AddFloat(2, 2, stride, 6 * sizeof(float)); // uv
        layout.AddFloat(3, 3, stride, 8 * sizeof(float)); // tangent

        // 创建 Mesh
        return Mesh(vertices.data(), vertices.size(), indices.data(), indices.size(), layout);
    }

    Mesh CreatePlane(float size, int subdivisions)
    {
        if (subdivisions < 1)
        {
            throw MeshException("Plane subdivisions must be >= 1");
        }

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        float halfSize = size / 2.0f;
        float step = size / subdivisions;

        // 生成顶点（Y = 0 平面，法线朝上）
        for (int z = 0; z <= subdivisions; ++z)
        {
            for (int x = 0; x <= subdivisions; ++x)
            {
                Vertex vertex;

                vertex.position.x = -halfSize + x * step;
                vertex.position.y = 0.0f;
                vertex.position.z = -halfSize + z * step;

                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);

                vertex.uv.x = static_cast<float>(x) / subdivisions;
                vertex.uv.y = static_cast<float>(z) / subdivisions;

                vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f); // X 轴方向

                vertices.push_back(vertex);
            }
        }

        // 生成索引
        for (int z = 0; z < subdivisions; ++z)
        {
            for (int x = 0; x < subdivisions; ++x)
            {
                int topLeft = z * (subdivisions + 1) + x;
                int topRight = topLeft + 1;
                int bottomLeft = (z + 1) * (subdivisions + 1) + x;
                int bottomRight = bottomLeft + 1;

                // 第一个三角形
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                // 第二个三角形
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        // 构建顶点布局：position(vec3), normal(vec3), uv(vec2), tangent(vec3)
        // 总步长：11 * sizeof(float)
        constexpr size_t stride = 11 * sizeof(float);
        VertexLayout layout;
        layout.AddFloat(0, 3, stride, 0);                 // position
        layout.AddFloat(1, 3, stride, 3 * sizeof(float)); // normal
        layout.AddFloat(2, 2, stride, 6 * sizeof(float)); // uv
        layout.AddFloat(3, 3, stride, 8 * sizeof(float)); // tangent

        return Mesh(vertices.data(), vertices.size(), indices.data(), indices.size(), layout);
    }

} // namespace zk_pbr::gfx::GeometryUtils

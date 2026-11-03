#include "GltfLoader.h"

#include <cmath>
#include <iostream>
#include <string>

#ifndef RENDERINGCUBE_TEST_MODEL
#define RENDERINGCUBE_TEST_MODEL "asset/su_57.glb"
#endif

namespace
{
bool nearlyEqual(float a, float b, float epsilon)
{
    return std::fabs(a - b) <= epsilon;
}
} // namespace

int main()
{
    GltfAsset asset;
    std::string error;
    if (!loadGltf(RENDERINGCUBE_TEST_MODEL, asset, error))
    {
        std::cerr << "[GltfLoaderTest] load failed: " << error << '\n';
        return 1;
    }

    if (asset.parts.empty())
    {
        std::cerr << "[GltfLoaderTest] no parts\n";
        return 1;
    }

    size_t vertexCount = 0;
    size_t indexCount = 0;
    for (const GltfPartData& part : asset.parts)
    {
        if (part.positions.empty())
        {
            std::cerr << "[GltfLoaderTest] empty positions\n";
            return 1;
        }
        if (part.normals.size() != part.positions.size() || part.colors.size() != part.positions.size())
        {
            std::cerr << "[GltfLoaderTest] vertex attribute arrays do not match\n";
            return 1;
        }
        if (part.indices.empty() || part.indices.size() % 3 != 0)
        {
            std::cerr << "[GltfLoaderTest] indices are empty or not triangle-aligned\n";
            return 1;
        }

        for (unsigned int index : part.indices)
        {
            if (index >= part.positions.size())
            {
                std::cerr << "[GltfLoaderTest] index out of range: " << index << '\n';
                return 1;
            }
        }

        for (const glm::vec3& normal : part.normals)
        {
            if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) ||
                !nearlyEqual(glm::length(normal), 1.0f, 1.0e-3f))
            {
                std::cerr << "[GltfLoaderTest] invalid normal\n";
                return 1;
            }
        }

        vertexCount += part.positions.size();
        indexCount += part.indices.size();
    }

    // 这些数字由随仓库提供的 asset/su_57.glb 固定下来；换模型时同步更新。
    if (vertexCount != 749628 || indexCount != 4497804)
    {
        std::cerr << "[GltfLoaderTest] unexpected counts: vertices=" << vertexCount << " indices=" << indexCount
                  << '\n';
        return 1;
    }

    const glm::vec3 expectedMin(-0.7602f, 0.0f, -0.3825f);
    const glm::vec3 expectedMax(0.4356f, 0.1965f, 0.3824f);
    if (glm::length(asset.boundsMin - expectedMin) > 1.0e-3f ||
        glm::length(asset.boundsMax - expectedMax) > 1.0e-3f)
    {
        std::cerr << "[GltfLoaderTest] unexpected bounds: min=(" << asset.boundsMin.x << ", " << asset.boundsMin.y
                  << ", " << asset.boundsMin.z << ") max=(" << asset.boundsMax.x << ", " << asset.boundsMax.y
                  << ", " << asset.boundsMax.z << ")\n";
        return 1;
    }

    std::cout << "[GltfLoaderTest] ok: parts=" << asset.parts.size() << " vertices=" << vertexCount
              << " indices=" << indexCount << '\n';
    return 0;
}

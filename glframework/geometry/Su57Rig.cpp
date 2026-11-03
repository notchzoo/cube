#include "Su57Rig.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr int kSurfaceCount = 10;

enum SurfaceId
{
    FlaperonRight = 1,
    FlaperonLeft,
    LeadingEdgeFlapRight,
    LeadingEdgeFlapLeft,
    StabilatorRight,
    StabilatorLeft,
    RudderRight,
    RudderLeft,
    NozzleRight,
    NozzleLeft,
};

bool inside(float value, float low, float high)
{
    return value > low && value < high;
}

int classifyTriangle(const glm::vec3& center)
{
    const float x = center.x;
    const float y = center.y;
    const float z = center.z;
    const float absZ = std::abs(z);

    // 优先级：方向舵 > 喷口 > 平尾 > 副翼/襟副翼 > 前缘襟翼。
    if (inside(x, 0.198f, 0.285f) && inside(absZ, 0.13f, 0.19f) && y > 0.108f)
        return z > 0.0f ? RudderRight : RudderLeft;
    if (inside(x, 0.349f, 0.436f) && inside(absZ, 0.02f, 0.09f) && inside(y, 0.02f, 0.10f))
        return z > 0.0f ? NozzleRight : NozzleLeft;
    if (inside(x, 0.267f, 0.42f) && inside(absZ, 0.10f, 0.28f) && inside(y, 0.015f, 0.13f))
        return z > 0.0f ? StabilatorRight : StabilatorLeft;
    if (inside(x, 0.10f, 0.22f) && inside(absZ, 0.18f, 0.36f) && inside(y, 0.015f, 0.10f))
        return z > 0.0f ? FlaperonRight : FlaperonLeft;
    if (inside(x, -0.36f, -0.05f) && inside(absZ, 0.18f, 0.34f) && inside(y, 0.015f, 0.10f))
        return z > 0.0f ? LeadingEdgeFlapRight : LeadingEdgeFlapLeft;
    return 0; // 机身
}

void configureSurface(int id, Su57SurfaceData& surface)
{
    surface.id = id;
    switch (id)
    {
    case FlaperonRight:
    case FlaperonLeft:
        surface.hingePoint = glm::vec3(0.12f, 0.058f, 0.0f);
        surface.hingeAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        surface.minAngle = -0.436f;
        surface.maxAngle = 0.436f;
        break;
    case LeadingEdgeFlapRight:
    case LeadingEdgeFlapLeft:
        surface.hingePoint = glm::vec3(-0.05f, 0.058f, 0.0f);
        surface.hingeAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        surface.minAngle = 0.0f;
        surface.maxAngle = 0.35f;
        break;
    case StabilatorRight:
    case StabilatorLeft:
        surface.hingePoint = glm::vec3(0.27f, 0.06f, 0.0f);
        surface.hingeAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        surface.minAngle = -0.50f;
        surface.maxAngle = 0.50f;
        break;
    case RudderRight:
    case RudderLeft:
        surface.hingePoint = glm::vec3(0.198f, 0.15f, id == RudderRight ? 0.155f : -0.155f);
        surface.hingeAxis = glm::vec3(0.0f, 1.0f, 0.0f);
        surface.minAngle = -0.50f;
        surface.maxAngle = 0.50f;
        break;
    case NozzleRight:
    case NozzleLeft:
        surface.hingePoint = glm::vec3(0.36f, 0.05f, id == NozzleRight ? 0.055f : -0.055f);
        surface.hingeAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        surface.minAngle = -0.25f;
        surface.maxAngle = 0.25f;
        break;
    default:
        break;
    }
}

GltfPartData buildPart(const GltfPartData& source, const std::vector<unsigned int>& triangles)
{
    GltfPartData part;
    part.material = source.material;
    part.nodeTransform = source.nodeTransform;

    std::vector<int> remap(source.positions.size(), -1);
    part.indices.reserve(triangles.size());
    for (unsigned int index : triangles)
    {
        const int oldIndex = static_cast<int>(index);
        if (remap[oldIndex] < 0)
        {
            remap[oldIndex] = static_cast<int>(part.positions.size());
            part.positions.push_back(source.positions[index]);
            part.normals.push_back(source.normals[index]);
            part.colors.push_back(source.colors[index]);
        }
        part.indices.push_back(static_cast<unsigned int>(remap[oldIndex]));
    }
    return part;
}
} // namespace

Su57SplitData splitSu57(const GltfPartData& source)
{
    Su57SplitData result;
    if (source.positions.empty() || source.indices.size() < 3)
        return result;

    std::vector<unsigned int> triangles[kSurfaceCount + 1];
    const size_t triangleCount = source.indices.size() / 3;
    for (size_t triangle = 0; triangle < triangleCount; ++triangle)
    {
        const unsigned int i0 = source.indices[triangle * 3 + 0];
        const unsigned int i1 = source.indices[triangle * 3 + 1];
        const unsigned int i2 = source.indices[triangle * 3 + 2];
        const glm::vec3 center = (source.positions[i0] + source.positions[i1] + source.positions[i2]) / 3.0f;
        const int id = classifyTriangle(center);
        triangles[id].push_back(i0);
        triangles[id].push_back(i1);
        triangles[id].push_back(i2);
    }

    result.body = buildPart(source, triangles[0]);
    for (int id = 1; id <= kSurfaceCount; ++id)
    {
        if (triangles[id].empty())
            continue;
        Su57SurfaceData surface;
        surface.geometry = buildPart(source, triangles[id]);
        configureSurface(id, surface);
        result.surfaces.push_back(std::move(surface));
    }
    return result;
}

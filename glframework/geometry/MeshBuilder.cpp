#include "MeshBuilder.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Mesh.h"

namespace
{
glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point)
{
    const glm::vec4 transformed = transform * glm::vec4(point, 1.0f);
    return glm::vec3(transformed);
}
}

void MeshBuilder::addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color)
{
    const glm::vec3 edge0 = b - a;
    const glm::vec3 edge1 = c - a;
    const glm::vec3 normal = glm::normalize(glm::cross(edge0, edge1));
    if (!std::isfinite(normal.x))
    {
        return;
    }

    const unsigned int base = static_cast<unsigned int>(m_positions.size());
    m_positions.push_back(a);
    m_positions.push_back(b);
    m_positions.push_back(c);
    m_colors.push_back(color);
    m_colors.push_back(color);
    m_colors.push_back(color);
    m_normals.push_back(normal);
    m_normals.push_back(normal);
    m_normals.push_back(normal);
    m_indices.push_back(base + 0);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
}

void MeshBuilder::addTriangleOriented(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                                      const glm::vec3& color, const glm::vec3& outwardCenter)
{
    const glm::vec3 normal = glm::cross(b - a, c - a);
    const glm::vec3 centroid = (a + b + c) / 3.0f;
    if (glm::dot(normal, centroid - outwardCenter) < 0.0f)
    {
        addTriangle(a, c, b, color);
        return;
    }
    addTriangle(a, b, c, color);
}

void MeshBuilder::addQuadOriented(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                                  const glm::vec3& color, const glm::vec3& outwardCenter)
{
    addTriangleOriented(a, b, c, color, outwardCenter);
    addTriangleOriented(a, c, d, color, outwardCenter);
}

void MeshBuilder::addBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec3& color)
{
    const glm::vec3 center = transformPoint(transform, glm::vec3(0.0f));

    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
        for (int sign = -1; sign <= 1; sign += 2)
        {
            glm::vec3 normal(0.0f);
            normal[axisIndex] = static_cast<float>(sign);

            glm::vec3 tangent(0.0f);
            tangent[(axisIndex + 1) % 3] = 1.0f;
            const glm::vec3 bitangent = glm::cross(normal, tangent);

            const float halfAlongNormal = halfExtents[axisIndex];
            const float halfAlongTangent = halfExtents[(axisIndex + 1) % 3];
            const float halfAlongBitangent = halfExtents[(axisIndex + 2) % 3];

            const glm::vec3 faceCenter = normal * halfAlongNormal;
            const glm::vec3 corners[4] = {
                faceCenter - tangent * halfAlongTangent - bitangent * halfAlongBitangent,
                faceCenter + tangent * halfAlongTangent - bitangent * halfAlongBitangent,
                faceCenter + tangent * halfAlongTangent + bitangent * halfAlongBitangent,
                faceCenter - tangent * halfAlongTangent + bitangent * halfAlongBitangent,
            };

            addQuadOriented(transformPoint(transform, corners[0]), transformPoint(transform, corners[1]),
                            transformPoint(transform, corners[2]), transformPoint(transform, corners[3]), color,
                            center);
        }
    }
}

void MeshBuilder::addCylinder(const glm::mat4& transform, float radius, float height, int segments,
                              const glm::vec3& color)
{
    segments = std::max(segments, 3);
    const float halfHeight = height * 0.5f;
    const glm::vec3 axisCenter = transformPoint(transform, glm::vec3(0.0f));
    const float step = 6.28318530717958647692f / static_cast<float>(segments);

    for (int index = 0; index < segments; ++index)
    {
        const float angle0 = step * static_cast<float>(index);
        const float angle1 = step * static_cast<float>(index + 1);

        const glm::vec3 low0(std::cos(angle0) * radius, -halfHeight, std::sin(angle0) * radius);
        const glm::vec3 low1(std::cos(angle1) * radius, -halfHeight, std::sin(angle1) * radius);
        const glm::vec3 high0(std::cos(angle0) * radius, halfHeight, std::sin(angle0) * radius);
        const glm::vec3 high1(std::cos(angle1) * radius, halfHeight, std::sin(angle1) * radius);

        addQuadOriented(transformPoint(transform, low0), transformPoint(transform, low1),
                        transformPoint(transform, high1), transformPoint(transform, high0), color, axisCenter);
    }

    // 两端封盖：扇形三角形，朝外提示取各自端面中心。
    const glm::vec3 bottomCenter = transformPoint(transform, glm::vec3(0.0f, -halfHeight, 0.0f));
    const glm::vec3 topCenter = transformPoint(transform, glm::vec3(0.0f, halfHeight, 0.0f));
    for (int index = 0; index < segments; ++index)
    {
        const float angle0 = step * static_cast<float>(index);
        const float angle1 = step * static_cast<float>(index + 1);

        const glm::vec3 low0(std::cos(angle0) * radius, -halfHeight, std::sin(angle0) * radius);
        const glm::vec3 low1(std::cos(angle1) * radius, -halfHeight, std::sin(angle1) * radius);
        const glm::vec3 high0(std::cos(angle0) * radius, halfHeight, std::sin(angle0) * radius);
        const glm::vec3 high1(std::cos(angle1) * radius, halfHeight, std::sin(angle1) * radius);

        addTriangleOriented(bottomCenter, transformPoint(transform, low0), transformPoint(transform, low1), color,
                            axisCenter);
        addTriangleOriented(topCenter, transformPoint(transform, high0), transformPoint(transform, high1), color,
                            axisCenter);
    }
}

void MeshBuilder::addLoft(const glm::mat4& transform, const std::vector<LoftRing>& rings, int segments,
                          const glm::vec3& color)
{
    if (rings.size() < 2)
    {
        return;
    }
    segments = std::max(segments, 3);

    std::vector<LoftRing> sorted = rings;
    std::sort(sorted.begin(), sorted.end(), [](const LoftRing& lhs, const LoftRing& rhs) { return lhs.x < rhs.x; });

    const float step = 6.28318530717958647692f / static_cast<float>(segments);

    const auto ringPoint = [](const LoftRing& ring, float angle)
    { return glm::vec3(ring.x, ring.yCenter + ring.halfHeight * std::sin(angle), ring.halfWidth * std::cos(angle)); };

    for (size_t ringIndex = 0; ringIndex + 1 < sorted.size(); ++ringIndex)
    {
        const LoftRing& current = sorted[ringIndex];
        const LoftRing& next = sorted[ringIndex + 1];
        const glm::vec3 axisPoint = transformPoint(
            transform, glm::vec3((current.x + next.x) * 0.5f, (current.yCenter + next.yCenter) * 0.5f, 0.0f));

        for (int segment = 0; segment < segments; ++segment)
        {
            const float angle0 = step * static_cast<float>(segment);
            const float angle1 = step * static_cast<float>(segment + 1);

            addQuadOriented(transformPoint(transform, ringPoint(current, angle0)),
                            transformPoint(transform, ringPoint(current, angle1)),
                            transformPoint(transform, ringPoint(next, angle1)),
                            transformPoint(transform, ringPoint(next, angle0)), color, axisPoint);
        }
    }

    // 首尾封盖。
    for (int end = 0; end < 2; ++end)
    {
        const LoftRing& ring = (end == 0) ? sorted.front() : sorted.back();
        const glm::vec3 center = transformPoint(transform, glm::vec3(ring.x, ring.yCenter, 0.0f));
        // The orientation reference must be inside the volume, not on the cap:
        // a coplanar reference gives a zero dot product for both windings.
        const float innerX = end == 0 ? sorted[1].x : sorted[sorted.size() - 2].x;
        const glm::vec3 inside = transformPoint(transform, glm::vec3(innerX, ring.yCenter, 0.0f));
        for (int segment = 0; segment < segments; ++segment)
        {
            const float angle0 = step * static_cast<float>(segment);
            const float angle1 = step * static_cast<float>(segment + 1);
            addTriangleOriented(center, transformPoint(transform, ringPoint(ring, angle0)),
                                transformPoint(transform, ringPoint(ring, angle1)), color, inside);
        }
    }
}

void MeshBuilder::addSmoothTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 na, glm::vec3 nb, glm::vec3 nc,
                                    const glm::vec3& color)
{
    if (glm::dot(glm::cross(b - a, c - a), na + nb + nc) < 0)
    {
        std::swap(b, c);
        std::swap(nb, nc);
    }
    const size_t first = m_positions.size();
    addTriangle(a, b, c, color);
    if (m_positions.size() == first + 3)
    {
        m_normals[first] = glm::normalize(na);
        m_normals[first + 1] = glm::normalize(nb);
        m_normals[first + 2] = glm::normalize(nc);
    }
}

void MeshBuilder::addSmoothCylinder(const glm::mat4& transform, float radius, float height, int segments,
                                    const glm::vec3& color)
{
    if (radius <= 0 || height <= 0)
        return;
    segments = std::max(segments, 3);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    const auto center = transformPoint(transform, glm::vec3(0));
    const auto low = transformPoint(transform, {0, -height * .5f, 0});
    const auto high = transformPoint(transform, {0, height * .5f, 0});
    for (int i = 0; i < segments; ++i)
    {
        const float a = 6.28318530718f * i / segments, c = 6.28318530718f * (i + 1) / segments;
        const glm::vec3 r0(std::cos(a), 0, std::sin(a)), r1(std::cos(c), 0, std::sin(c));
        const auto p0 = transformPoint(transform, r0 * radius + glm::vec3(0, -height * .5f, 0));
        const auto p1 = transformPoint(transform, r1 * radius + glm::vec3(0, -height * .5f, 0));
        const auto p2 = transformPoint(transform, r1 * radius + glm::vec3(0, height * .5f, 0));
        const auto p3 = transformPoint(transform, r0 * radius + glm::vec3(0, height * .5f, 0));
        const auto n0 = glm::normalize(normalMatrix * r0), n1 = glm::normalize(normalMatrix * r1);
        addSmoothTriangle(p0, p1, p2, n0, n1, n1, color);
        addSmoothTriangle(p0, p2, p3, n0, n1, n0, color);
        // Separate cap vertices keep the circular rim sharp.
        addTriangleOriented(low, p0, p1, color, center);
        addTriangleOriented(high, p3, p2, color, center);
    }
}

void MeshBuilder::addSmoothLoft(const glm::mat4& transform, const std::vector<LoftRing>& rings, int segments,
                                const glm::vec3& color, int axialSteps)
{
    if (rings.size() < 2)
        return;
    auto sorted = rings;
    std::sort(sorted.begin(), sorted.end(), [](const LoftRing& a, const LoftRing& c) { return a.x < c.x; });
    for (size_t i = 0; i < sorted.size(); ++i)
        if (sorted[i].halfWidth <= 0 || sorted[i].halfHeight <= 0 || (i > 0 && sorted[i].x <= sorted[i - 1].x))
            return;
    segments = std::max(segments, 3);
    axialSteps = std::max(axialSteps, 1);
    // Monotone cubic tangents avoid the bulges and negative radii of Catmull-Rom
    // on long cylindrical sections meeting short, tapering end sections.
    const auto value = [](const LoftRing& r) { return glm::vec3(r.halfWidth, r.halfHeight, r.yCenter); };
    std::vector<glm::vec3> tangents(sorted.size());
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        if (i == 0)
            tangents[i] = (value(sorted[1]) - value(sorted[0])) / (sorted[1].x - sorted[0].x);
        else if (i + 1 == sorted.size())
            tangents[i] = (value(sorted[i]) - value(sorted[i - 1])) / (sorted[i].x - sorted[i - 1].x);
        else
        {
            const float h0 = sorted[i].x - sorted[i - 1].x, h1 = sorted[i + 1].x - sorted[i].x;
            const auto d0 = (value(sorted[i]) - value(sorted[i - 1])) / h0;
            const auto d1 = (value(sorted[i + 1]) - value(sorted[i])) / h1;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float w0 = 2 * h1 + h0, w1 = h1 + 2 * h0;
                tangents[i][axis] = d0[axis] * d1[axis] <= 0 ? 0 : (w0 + w1) / (w0 / d0[axis] + w1 / d1[axis]);
            }
        }
    }
    struct Section
    {
        float x;
        glm::vec3 shape, derivative;
    };
    std::vector<Section> samples;
    for (size_t i = 0; i + 1 < sorted.size(); ++i)
    {
        const float h = sorted[i + 1].x - sorted[i].x;
        for (int step = 0; step < axialSteps; ++step)
        {
            const float t = static_cast<float>(step) / axialSteps, t2 = t * t, t3 = t2 * t;
            const auto a = value(sorted[i]), c = value(sorted[i + 1]);
            const auto shape = (2 * t3 - 3 * t2 + 1) * a + (t3 - 2 * t2 + t) * h * tangents[i] +
                               (-2 * t3 + 3 * t2) * c + (t3 - t2) * h * tangents[i + 1];
            const auto derivative = ((6 * t2 - 6 * t) * a + (3 * t2 - 4 * t + 1) * h * tangents[i] +
                                     (-6 * t2 + 6 * t) * c + (3 * t2 - 2 * t) * h * tangents[i + 1]) /
                                    h;
            samples.push_back({sorted[i].x + t * h, shape, derivative});
        }
    }
    samples.push_back({sorted.back().x, value(sorted.back()), tangents.back()});
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    const auto point = [&](const Section& r, int j)
    {
        const float angle = 6.28318530718f * (j % segments) / segments;
        return transformPoint(transform, {r.x, r.shape.z + r.shape.y * std::sin(angle), r.shape.x * std::cos(angle)});
    };
    const auto normal = [&](const Section& r, int j)
    {
        const float angle = 6.28318530718f * (j % segments) / segments;
        const glm::vec3 along(1, r.derivative.z + r.derivative.y * std::sin(angle), r.derivative.x * std::cos(angle));
        const glm::vec3 around(0, r.shape.y * std::cos(angle), -r.shape.x * std::sin(angle));
        return glm::normalize(normalMatrix * glm::cross(along, around));
    };
    for (size_t i = 0; i + 1 < samples.size(); ++i)
        for (int j = 0; j < segments; ++j)
        {
            const auto& a = samples[i];
            const auto& c = samples[i + 1];
            const auto p0 = point(a, j), p1 = point(a, j + 1), p2 = point(c, j + 1), p3 = point(c, j);
            const auto n0 = normal(a, j), n1 = normal(a, j + 1), n2 = normal(c, j + 1), n3 = normal(c, j);
            addSmoothTriangle(p0, p1, p2, n0, n1, n2, color);
            addSmoothTriangle(p0, p2, p3, n0, n2, n3, color);
        }
    for (int end = 0; end < 2; ++end)
    {
        const auto& r = end == 0 ? samples.front() : samples.back();
        const auto& inner = end == 0 ? samples[1] : samples[samples.size() - 2];
        const auto center = transformPoint(transform, {r.x, r.shape.z, 0});
        const auto inside = transformPoint(transform, {inner.x, r.shape.z, 0});
        for (int j = 0; j < segments; ++j)
            addTriangleOriented(center, point(r, j), point(r, j + 1), color, inside);
    }
}

void MeshBuilder::addTorus(const glm::mat4& transform, float majorRadius, float tubeRadius, int radialSegments,
                           int tubeSegments, const glm::vec3& color)
{
    if (majorRadius <= tubeRadius || tubeRadius <= 0)
        return;
    radialSegments = std::max(radialSegments, 3);
    tubeSegments = std::max(tubeSegments, 3);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    const auto sample = [&](int u, int v, glm::vec3& p, glm::vec3& n)
    {
        const float a = 6.28318530718f * (u % radialSegments) / radialSegments;
        const float c = 6.28318530718f * (v % tubeSegments) / tubeSegments;
        const glm::vec3 radial(std::cos(a), 0, std::sin(a));
        const auto localNormal = radial * std::cos(c) + glm::vec3(0, std::sin(c), 0);
        p = transformPoint(transform, radial * majorRadius + localNormal * tubeRadius);
        n = glm::normalize(normalMatrix * localNormal);
    };
    for (int u = 0; u < radialSegments; ++u)
        for (int v = 0; v < tubeSegments; ++v)
        {
            glm::vec3 p0, p1, p2, p3, n0, n1, n2, n3;
            sample(u, v, p0, n0);
            sample(u + 1, v, p1, n1);
            sample(u + 1, v + 1, p2, n2);
            sample(u, v + 1, p3, n3);
            addSmoothTriangle(p0, p1, p2, n0, n1, n2, color);
            addSmoothTriangle(p0, p2, p3, n0, n2, n3, color);
        }
}

void MeshBuilder::appendTransformed(const MeshBuilder& source, const glm::mat4& transform)
{
    // Copy also makes self-appending safe while this builder grows.
    const auto positions = source.m_positions;
    const auto colors = source.m_colors;
    const auto normals = source.m_normals;
    const auto indices = source.m_indices;
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
        addSmoothTriangle(
            transformPoint(transform, positions[indices[i]]), transformPoint(transform, positions[indices[i + 1]]),
            transformPoint(transform, positions[indices[i + 2]]), normalMatrix * normals[indices[i]],
            normalMatrix * normals[indices[i + 1]], normalMatrix * normals[indices[i + 2]], colors[indices[i]]);
}

void MeshBuilder::build(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& colors,
                        std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices) const
{
    positions = m_positions;
    colors = m_colors;
    normals = m_normals;
    indices = m_indices;
}

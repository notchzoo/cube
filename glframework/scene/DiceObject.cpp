#include "DiceObject.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

std::array<PBRMaterial, DiceObject::kFaceCount> defaultFaceMaterials()
{
    return { {
        { glm::vec3(0.86f, 0.20f, 0.16f), 0.0f, 0.35f, 1.0f, 1.0f },  // +Z 红
        { glm::vec3(0.20f, 0.62f, 0.30f), 0.0f, 0.55f, 1.0f, 1.0f },  // -Z 绿
        { glm::vec3(0.18f, 0.38f, 0.84f), 0.0f, 0.22f, 1.0f, 1.0f },  // +X 蓝
        { glm::vec3(0.93f, 0.70f, 0.18f), 0.0f, 0.45f, 1.0f, 1.0f },  // -X 琥珀
        { glm::vec3(0.88f, 0.88f, 0.86f), 0.0f, 0.12f, 1.0f, 1.0f },  // +Y 米白
        { glm::vec3(0.55f, 0.30f, 0.78f), 0.9f, 0.28f, 1.0f, 1.0f },  // -Y 紫色金属
    } };
}
}

std::shared_ptr<Mesh> DiceObject::createCubeMesh()
{
    // 硬边立方体：每个面 4 个独立顶点，法线在面内恒定。
    const glm::vec3 faceNormals[kFaceCount] = {
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
    };

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
    positions.reserve(24);
    colors.reserve(24);
    normals.reserve(24);
    indices.reserve(36);

    for (const glm::vec3& normal : faceNormals)
    {
        // tangent x bitangent = normal，保证每个面绕序一致（CCW 朝外）。
        const glm::vec3 tangent = (std::abs(normal.y) > 0.9f)
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), normal));
        const glm::vec3 bitangent = glm::cross(normal, tangent);

        const unsigned int base = static_cast<unsigned int>(positions.size());
        const glm::vec3 corners[4] = {
            -tangent - bitangent,
            tangent - bitangent,
            tangent + bitangent,
            -tangent + bitangent,
        };

        for (const glm::vec3& corner : corners)
        {
            // 沿法线偏移，面才落在立方体表面而不是穿过中心。
            // 单位立方体：对象空间坐标保持在 [-1, 1]，点数布局不依赖模型缩放。
            positions.push_back(corner + normal);
            normals.push_back(normal);
            colors.push_back(glm::vec3(1.0f));
        }

        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    const auto mesh = std::make_shared<Mesh>();
    mesh->upload(positions, colors, normals, indices);
    return mesh;
}

DiceObject::DiceObject(std::shared_ptr<Mesh> mesh,
                       float centerHeight,
                       float scale,
                       float initialAngleRadians,
                       const glm::vec3& spinAxis)
    : m_mesh(std::move(mesh))
    , m_faces(defaultFaceMaterials())
    , m_centerHeight(centerHeight)
    , m_scale(scale)
    , m_angle(initialAngleRadians)
    , m_axis(spinAxis)
{
    wrapAngle();
    rebuildModel();
}

void DiceObject::setFaceMaterial(int face, const PBRMaterial& material)
{
    if (face < 0 || face >= kFaceCount)
    {
        return;
    }
    m_faces[static_cast<size_t>(face)] = material.clamped();
}

const PBRMaterial& DiceObject::faceMaterial(int face) const
{
    static const PBRMaterial fallback{};
    if (face < 0 || face >= kFaceCount)
    {
        return fallback;
    }
    return m_faces[static_cast<size_t>(face)];
}

void DiceObject::setAllFaceMaterials(const PBRMaterial& material)
{
    const PBRMaterial sanitized = material.clamped();
    for (PBRMaterial& face : m_faces)
    {
        face = sanitized;
    }
}

void DiceObject::setOpacity(float opacity)
{
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
}

void DiceObject::setPips(const std::array<int, kFaceCount>& pips)
{
    for (size_t i = 0; i < m_pips.size(); ++i)
    {
        m_pips[i] = std::clamp(pips[i], 0, 6);
    }
}

void DiceObject::setSpinSpeed(float radiansPerSecond)
{
    // 允许负值（反向旋转），只挡掉离谱量级。
    m_speed = std::clamp(radiansPerSecond, -20.0f, 20.0f);
}

void DiceObject::setSpinAngle(float radians)
{
    m_angle = radians;
    wrapAngle();
    rebuildModel();
}

void DiceObject::setSpinAxis(const glm::vec3& axis)
{
    if (glm::length(axis) < 1e-5f)
    {
        return;
    }
    m_axis = axis;
    rebuildModel();
}

void DiceObject::update(float deltaSeconds)
{
    if (!m_spinEnabled)
    {
        return;
    }
    m_angle += m_speed * deltaSeconds;
    wrapAngle();
    rebuildModel();
}

void DiceObject::wrapAngle()
{
    m_angle = std::fmod(m_angle, kTwoPi);
    if (m_angle < 0.0f)
    {
        m_angle += kTwoPi;
    }
}

void DiceObject::rebuildModel()
{
    // 平移 -> 绕 Y 自转 -> 缩放（单位立方体 -> 边长 2*scale）。
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, m_centerHeight, 0.0f));
    const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), m_angle, m_axis);
    const glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
    m_model = translation * rotation * scale;
}

void DiceObject::appendDrawItems(std::vector<DrawItem>& items) const
{
    if (!m_mesh || !m_mesh->valid())
    {
        return;
    }

    for (int face = 0; face < kFaceCount; ++face)
    {
        DrawItem item;
        item.mesh = m_mesh.get();
        item.model = m_model;
        item.material = m_faces[static_cast<size_t>(face)];
        // 全局不透明度与每面自身不透明度相乘。
        item.material.opacity = std::clamp(item.material.opacity * m_opacity, 0.0f, 1.0f);
        item.pipCount = m_pipsEnabled ? m_pips[static_cast<size_t>(face)] : 0;
        item.firstIndex = face * kIndicesPerFace;
        item.indexCount = kIndicesPerFace;
        item.shadowCull = DrawItem::FaceCull::Front;
        items.push_back(item);
    }
}

void DiceObject::appendShadowItems(std::vector<DrawItem>& items) const
{
    if (!m_mesh || !m_mesh->valid())
    {
        return;
    }

    // 闭合体：整个网格只画一次背面写深度（正面被剔除，避免自阴影 acne）。
    DrawItem item;
    item.mesh = m_mesh.get();
    item.model = m_model;
    item.material = m_faces[0];
    item.pipCount = 0;
    item.firstIndex = 0;
    item.indexCount = 0;      // 整网格
    item.shadowCull = DrawItem::FaceCull::Front;
    items.push_back(item);
}
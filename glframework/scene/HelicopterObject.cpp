#include "HelicopterObject.h"

#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#include "MeshBuilder.h"

namespace
{
// 局部坐标：+X 机头，+Y 向上，+Z 右侧；单位近似米。
const glm::vec3 kBodyColor(0.66f, 0.12f, 0.10f);  // 救援涂装红
const glm::vec3 kDarkColor(0.09f, 0.10f, 0.11f);  // 旋翼/滑橇
const glm::vec3 kGlassColor(0.12f, 0.19f, 0.25f); // 座舱玻璃
const glm::vec3 kMetalColor(0.34f, 0.35f, 0.37f); // 旋翼轴

constexpr float kDegreesToRadians = 0.01745329251994329577f;

glm::mat4 translate(const glm::vec3& position)
{
    return glm::translate(glm::mat4(1.0f), position);
}

glm::mat4 rotateX(float degrees)
{
    return glm::rotate(glm::mat4(1.0f), degrees * kDegreesToRadians, glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::mat4 rotateY(float degrees)
{
    return glm::rotate(glm::mat4(1.0f), degrees * kDegreesToRadians, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 rotateZ(float degrees)
{
    return glm::rotate(glm::mat4(1.0f), degrees * kDegreesToRadians, glm::vec3(0.0f, 0.0f, 1.0f));
}
}

std::shared_ptr<Mesh> HelicopterObject::createMesh()
{
    MeshBuilder builder;

    // --- 机身：从尾到头的一组椭圆截面放样 ---------------------------------
    const std::vector<MeshBuilder::LoftRing> fuselage = {
        {-2.10f, 0.10f, 0.11f, 0.03f},                                                              // 尾端
        {-1.30f, 0.26f, 0.28f, 0.07f}, {-0.55f, 0.46f, 0.50f, 0.10f}, {0.25f, 0.52f, 0.54f, 0.10f}, // 最宽处
        {1.00f, 0.38f, 0.40f, 0.05f},  {1.55f, 0.18f, 0.20f, 0.00f},                                // 机头
    };
    builder.addLoft(glm::mat4(1.0f), fuselage, 12, kBodyColor);

    // --- 座舱：前风挡（后倾） + 顶部舷窗 -----------------------------------
    builder.addBox(translate(glm::vec3(1.08f, 0.30f, 0.0f)) * rotateZ(-34.0f), glm::vec3(0.24f, 0.13f, 0.30f),
                   kGlassColor);
    builder.addBox(translate(glm::vec3(0.70f, 0.48f, 0.0f)), glm::vec3(0.17f, 0.045f, 0.25f), kGlassColor);

    // --- 尾梁 + 垂尾 + 平尾 -------------------------------------------------
    builder.addCylinder(translate(glm::vec3(-2.72f, 0.03f, 0.0f)) * rotateZ(-90.0f), 0.085f, 1.40f, 10, kBodyColor);

    builder.addBox(translate(glm::vec3(-3.30f, 0.45f, 0.0f)) * rotateZ(-8.0f), glm::vec3(0.18f, 0.36f, 0.035f),
                   kBodyColor);

    builder.addBox(translate(glm::vec3(-2.95f, 0.10f, 0.0f)), glm::vec3(0.20f, 0.03f, 0.40f), kBodyColor);

    // --- 尾桨：绕 Z 轴的轮毂 + 两片桨叶 ------------------------------------
    builder.addCylinder(translate(glm::vec3(-3.30f, 0.52f, 0.10f)) * rotateX(90.0f), 0.05f, 0.12f, 8, kMetalColor);
    builder.addBox(translate(glm::vec3(-3.30f, 0.52f, 0.15f)), glm::vec3(0.34f, 0.025f, 0.012f), kDarkColor);
    builder.addBox(translate(glm::vec3(-3.30f, 0.52f, 0.15f)), glm::vec3(0.025f, 0.34f, 0.012f), kDarkColor);

    // --- 主旋翼：桅杆 + 轮毂 + 四片桨叶（两个十字长方体）-------------------
    builder.addCylinder(translate(glm::vec3(0.12f, 0.76f, 0.0f)), 0.055f, 0.30f, 10, kMetalColor);
    builder.addBox(translate(glm::vec3(0.12f, 0.93f, 0.0f)), glm::vec3(0.13f, 0.045f, 0.13f), kMetalColor);
    builder.addBox(translate(glm::vec3(0.12f, 0.96f, 0.0f)) * rotateY(6.0f), glm::vec3(0.95f, 0.012f, 0.065f),
                   kDarkColor);
    builder.addBox(translate(glm::vec3(0.12f, 0.96f, 0.0f)) * rotateY(96.0f), glm::vec3(0.95f, 0.012f, 0.065f),
                   kDarkColor);

    // --- 滑橇：两根纵梁 + 四根支柱 + 两根横管 ---------------------------------
    for (int side = -1; side <= 1; side += 2)
    {
        const float z = 0.40f * static_cast<float>(side);
        builder.addBox(translate(glm::vec3(0.0f, -0.66f, z)), glm::vec3(1.15f, 0.035f, 0.035f), kDarkColor);
        builder.addBox(translate(glm::vec3(0.55f, -0.42f, z)), glm::vec3(0.035f, 0.26f, 0.035f), kDarkColor);
        builder.addBox(translate(glm::vec3(-0.55f, -0.42f, z)), glm::vec3(0.035f, 0.26f, 0.035f), kDarkColor);
    }
    builder.addBox(translate(glm::vec3(0.55f, -0.42f, 0.0f)), glm::vec3(0.035f, 0.035f, 0.40f), kDarkColor);
    builder.addBox(translate(glm::vec3(-0.55f, -0.42f, 0.0f)), glm::vec3(0.035f, 0.035f, 0.40f), kDarkColor);

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
    builder.build(positions, colors, normals, indices);
    std::cout << "[Helicopter] mesh: " << positions.size() << " vertices, " << indices.size() / 3u << " triangles\n";

    const auto mesh = std::make_shared<Mesh>();
    mesh->upload(positions, colors, normals, indices);
    return mesh;
}

PBRMaterial HelicopterObject::defaultMaterial()
{
    PBRMaterial material;
    material.albedo = glm::vec3(1.0f);
    material.metallic = 0.22f;
    material.roughness = 0.38f;
    material.ao = 1.0f;
    material.opacity = 1.0f;
    return material;
}

HelicopterObject::HelicopterObject(std::shared_ptr<Mesh> mesh, const PBRMaterial& material, const glm::mat4& model)
    : m_mesh(std::move(mesh)),
      m_material(material.clamped()),
      m_model(model)
{
}

void HelicopterObject::appendDrawItems(std::vector<DrawItem>& items) const
{
    if (!m_mesh || !m_mesh->valid())
    {
        return;
    }

    DrawItem item;
    item.mesh = m_mesh.get();
    item.model = m_model;
    item.material = m_material;
    item.pipCount = 0;
    item.firstIndex = 0;
    item.indexCount = 0; // 整网格
    item.shadowCull = DrawItem::FaceCull::Front;
    items.push_back(item);
}

void HelicopterObject::appendShadowItems(std::vector<DrawItem>& items) const
{
    if (m_castsShadow)
    {
        appendDrawItems(items);
    }
}
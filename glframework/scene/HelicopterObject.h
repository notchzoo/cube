#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "PBRMaterial.h"
#include "SceneObject.h"

/// 程序化生成的低多边形直升机：机身放样 + 尾梁 + 主/尾旋翼 + 滑橇。
///
/// 只有一份网格、一套材质：部件靠顶点色区分（机身红、座舱玻璃、旋翼深灰）。
/// 顶点数约 1500，全部由 MeshBuilder 在启动时生成，不依赖任何外部模型文件。
class HelicopterObject : public SceneObject
{
public:
    static std::shared_ptr<Mesh> createMesh();

    /// 反照率取白色，让顶点色决定成品颜色；金属/粗糙度给"喷漆金属"的观感。
    static PBRMaterial defaultMaterial();

    HelicopterObject(std::shared_ptr<Mesh> mesh, const PBRMaterial& material, const glm::mat4& model = glm::mat4(1.0f));

    void setModelMatrix(const glm::mat4& model) { m_model = model; }
    const glm::mat4& modelMatrix() const { return m_model; }

    void setMaterial(const PBRMaterial& material) { m_material = material.clamped(); }
    const PBRMaterial& material() const { return m_material; }

    void setCastsShadow(bool casts) { m_castsShadow = casts; }
    bool castsShadow() const override { return m_castsShadow; }

    float opacity() const override { return m_material.opacity; }

    void appendDrawItems(std::vector<DrawItem>& items) const override;
    void appendShadowItems(std::vector<DrawItem>& items) const override;

private:
    std::shared_ptr<Mesh> m_mesh;
    PBRMaterial m_material;
    glm::mat4 m_model{1.0f};
    bool m_castsShadow = true;
};
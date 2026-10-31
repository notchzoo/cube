#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "PBRMaterial.h"
#include "SceneObject.h"

/// 通用"一个网格 + 一套材质"的对象：地面用它，以后加静态道具也用它。
class MeshObject : public SceneObject
{
public:
    /// 生成一块水平地面片（法线 +Y，四顶点两三角形）。
    static std::shared_ptr<Mesh> createGroundMesh(float halfExtent);

    MeshObject(std::shared_ptr<Mesh> mesh,
               const PBRMaterial& material,
               const glm::mat4& model = glm::mat4(1.0f));

    void setMaterial(const PBRMaterial& material) { m_material = material.clamped(); }
    const PBRMaterial& material() const { return m_material; }

    void setModelMatrix(const glm::mat4& model) { m_model = model; }
    const glm::mat4& modelMatrix() const { return m_model; }

    void setCastsShadow(bool casts) { m_castsShadow = casts; }
    bool castsShadow() const override { return m_castsShadow; }
    /// Closed volumes can cull front faces in the depth pass to prevent self-shadow acne.
    void setShadowCull(DrawItem::FaceCull cull) { m_shadowCull = cull; }

    float opacity() const override { return m_material.opacity; }

    void appendDrawItems(std::vector<DrawItem>& items) const override;
    void appendShadowItems(std::vector<DrawItem>& items) const override;

private:
    std::shared_ptr<Mesh> m_mesh;
    PBRMaterial m_material;
    glm::mat4 m_model{ 1.0f };
    bool m_castsShadow = true;
    DrawItem::FaceCull m_shadowCull = DrawItem::FaceCull::Back;
};

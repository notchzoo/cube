#include "MeshObject.h"

#include <vector>

std::shared_ptr<Mesh> MeshObject::createGroundMesh(float halfExtent)
{
    const auto mesh = std::make_shared<Mesh>();
    const std::vector<glm::vec3> positions = {
        glm::vec3(-halfExtent, 0.0f, -halfExtent),
        glm::vec3(-halfExtent, 0.0f, halfExtent),
        glm::vec3(halfExtent, 0.0f, halfExtent),
        glm::vec3(halfExtent, 0.0f, -halfExtent),
    };
    const std::vector<glm::vec3> normals(4, glm::vec3(0.0f, 1.0f, 0.0f));
    const std::vector<glm::vec3> colors(4, glm::vec3(1.0f));
    const std::vector<unsigned int> indices = {0, 1, 2, 0, 2, 3};

    mesh->upload(positions, colors, normals, indices);
    return mesh;
}

MeshObject::MeshObject(std::shared_ptr<Mesh> mesh, const PBRMaterial& material, const glm::mat4& model)
    : m_mesh(std::move(mesh)),
      m_material(material.clamped()),
      m_model(model)
{
}

void MeshObject::appendDrawItems(std::vector<DrawItem>& items) const
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
    item.shadowCull = m_shadowCull;
    items.push_back(item);
}

void MeshObject::appendShadowItems(std::vector<DrawItem>& items) const
{
    if (!m_castsShadow)
    {
        return;
    }
    appendDrawItems(items);
}

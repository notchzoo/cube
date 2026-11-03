#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "PBRMaterial.h"
#include "SceneObject.h"

/// 一个 glTF 模型：每个 primitive 一条 DrawItem。
///
/// 网格由 Application 在 GL 上下文建立后上传；本类只持有 GPU 资源并产出绘制项，
/// 不认识 PBRRenderer、阴影 pass 或 glTF 格式本身。
class GltfObject : public SceneObject
{
public:
    struct Part
    {
        std::shared_ptr<Mesh> mesh;
        PBRMaterial material;
        glm::mat4 nodeTransform{1.0f};
    };

    GltfObject(std::vector<Part> parts, const glm::mat4& placement)
        : m_parts(std::move(parts)), m_placement(placement)
    {
    }

    void appendDrawItems(std::vector<DrawItem>& items) const override;

    /// 1.5M 三角形的模型默认不参与阴影 pass；确认性能后再打开。
    void setCastsShadow(bool casts) { m_castsShadow = casts; }
    bool castsShadow() const override { return m_castsShadow; }

    float opacity() const override { return 1.0f; }

    const glm::mat4& placement() const { return m_placement; }
    /// 由 FixedWingObject 每帧写入整机 Model；静态模型也可以用它调整摆位。
    void setModelMatrix(const glm::mat4& model) { m_placement = model; }
    size_t partCount() const { return m_parts.size(); }

private:
    std::vector<Part> m_parts;
    glm::mat4 m_placement{1.0f};
    bool m_castsShadow = false;
};

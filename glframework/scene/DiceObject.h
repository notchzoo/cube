#pragma once

#include <array>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "PBRMaterial.h"
#include "SceneObject.h"

/// 一颗骰子：单位立方体的几何 + 六面材质 + 点数 + 自转状态。
///
/// 只依赖 Mesh / PBRMaterial / SceneObject，不认识 PBRRenderer、Shader 与光照；
/// 想加第二颗骰子，共享 createCubeMesh() 的网格即可。
class DiceObject : public SceneObject
{
public:
    static constexpr int kFaceCount = 6;
    static constexpr int kIndicesPerFace = 6;

    /// 单位立方体（顶点在 ±1）：24 顶点 / 36 索引。
    /// 面顺序固定为 +Z, -Z, +X, -X, +Y, -Y，与材质下标一一对应。
    static std::shared_ptr<Mesh> createCubeMesh();

    DiceObject(std::shared_ptr<Mesh> mesh,
               float centerHeight,
               float scale,
               float initialAngleRadians = 0.0f,
               const glm::vec3& spinAxis = glm::vec3(0.0f, 1.0f, 0.0f));

    // --- 材质 -------------------------------------------------------------
    void setFaceMaterial(int face, const PBRMaterial& material);
    const PBRMaterial& faceMaterial(int face) const;
    void setAllFaceMaterials(const PBRMaterial& material);

    void setOpacity(float opacity);
    float opacity() const override { return m_opacity; }

    // --- 点数 -------------------------------------------------------------
    void setPips(const std::array<int, kFaceCount>& pips);
    const std::array<int, kFaceCount>& pips() const { return m_pips; }
    void setPipsEnabled(bool enabled) { m_pipsEnabled = enabled; }
    bool pipsEnabled() const { return m_pipsEnabled; }

    // --- 自转 -------------------------------------------------------------
    void setSpinEnabled(bool enabled) { m_spinEnabled = enabled; }
    void toggleSpin() { m_spinEnabled = !m_spinEnabled; }
    bool spinEnabled() const { return m_spinEnabled; }
    void setSpinSpeed(float radiansPerSecond);
    /// 旋转轴按原样交给 glm::rotate（GLM 期望单位轴，这里保持调用方给的向量）。
    void setSpinAxis(const glm::vec3& axis);
    const glm::vec3& spinAxis() const { return m_axis; }
    void setSpinAngle(float radians);
    float spinAngle() const { return m_angle; }
    float spinSpeed() const { return m_speed; }

    /// 每帧调用：启用时推进角度并重建模型矩阵。
    void update(float deltaSeconds);

    const glm::mat4& modelMatrix() const { return m_model; }

    // --- SceneObject ------------------------------------------------------
    bool castsShadow() const override { return true; }
    void appendDrawItems(std::vector<DrawItem>& items) const override;
    void appendShadowItems(std::vector<DrawItem>& items) const override;

private:
    void wrapAngle();
    void rebuildModel();

    std::shared_ptr<Mesh> m_mesh;
    std::array<PBRMaterial, kFaceCount> m_faces;
    std::array<int, kFaceCount> m_pips{ 1, 2, 3, 4, 5, 6 };
    bool m_pipsEnabled = true;

    float m_opacity = 1.0f;
    float m_centerHeight = 0.805f;
    float m_scale = 0.8f;

    float m_angle = 0.0f;
    float m_speed = 0.6f;
    glm::vec3 m_axis{ 0.0f, 1.0f, 0.0f };
    bool m_spinEnabled = true;

    glm::mat4 m_model{ 1.0f };
};
#pragma once

#include <vector>

#include <glm/glm.hpp>

class Mesh;

/// 把程序化几何累加成一份 Mesh。
///
/// 约定：
///   * 基础几何保留平面法线；Smooth 接口生成曲面法线，端盖仍保留硬边；
///   * 顶点色用来区分机体 / 座舱 / 旋翼 / 滑橇，因此一份网格 + 一套材质即可多色；
///   * 所有朝向都按"面心相对 outwardCenter 的外法线"自动纠正绕序，
///     调用方不需要逐面手工保证 CCW。
class MeshBuilder
{
public:
    /// 放样用的椭圆截面：沿局部 X 排列，截面在 YZ 平面内。
    struct LoftRing
    {
        float x = 0.0f;
        float halfWidth = 0.1f;    // 局部 Z 半宽
        float halfHeight = 0.1f;   // 局部 Y 半高
        float yCenter = 0.0f;
    };

    void addBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec3& color);
    void addCylinder(const glm::mat4& transform, float radius, float height, int segments, const glm::vec3& color);
    void addLoft(const glm::mat4& transform, const std::vector<LoftRing>& rings, int segments, const glm::vec3& color);
    /// [曲面平滑] 圆柱侧面使用径向法线，端盖保持独立平面法线，避免轮缘被错误磨圆。
    void addSmoothCylinder(const glm::mat4& transform, float radius, float height, int segments, const glm::vec3& color);
    /// [曲面平滑] 截面间保形三次插值；segments 控制环向细分，axialSteps 控制相邻截面间细分。
    /// 法线由曲面切向量推导，不是每个三角形单独一条法线。
    void addSmoothLoft(const glm::mat4& transform, const std::vector<LoftRing>& rings,
                       int segments, const glm::vec3& color, int axialSteps = 8);
    /// [曲面平滑] 环面轴为局部 Y；用于圆润轮胎、轮毂和进气口边缘。
    void addTorus(const glm::mat4& transform, float majorRadius, float tubeRadius,
                  int radialSegments, int tubeSegments, const glm::vec3& color);
    /// 合并另一个 builder，支持正行列式的仿射变换；逆转置传递法线，保留非均匀缩放后的平滑效果。
    void appendTransformed(const MeshBuilder& source, const glm::mat4& transform);
    /// Flat polygon with automatic outward winding; reference must lie inside the object.
    void addQuad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                 const glm::vec3& color, const glm::vec3& outwardCenter)
    {
        addQuadOriented(a, b, c, d, color, outwardCenter);
    }

    bool empty() const { return m_indices.empty(); }

    void build(std::vector<glm::vec3>& positions,
               std::vector<glm::vec3>& colors,
               std::vector<glm::vec3>& normals,
               std::vector<unsigned int>& indices) const;

private:
    void addSmoothTriangle(glm::vec3 a, glm::vec3 b, glm::vec3 c,
                           glm::vec3 na, glm::vec3 nb, glm::vec3 nc, const glm::vec3& color);
    void addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color);
    void addTriangleOriented(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                             const glm::vec3& color, const glm::vec3& outwardCenter);
    void addQuadOriented(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                         const glm::vec3& color, const glm::vec3& outwardCenter);

    std::vector<glm::vec3> m_positions;
    std::vector<glm::vec3> m_colors;
    std::vector<glm::vec3> m_normals;
    std::vector<unsigned int> m_indices;
};

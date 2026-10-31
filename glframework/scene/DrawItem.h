#pragma once

#include <glm/glm.hpp>

#include "PBRMaterial.h"

class Mesh;

/// 一次绘制的完整描述：几何 + 变换 + 材质 + 可选的面/点数参数。
///
/// 这是"场景"与"渲染器"之间唯一的契约：
///   * 场景对象只负责产出 DrawItem，不认识 PBRRenderer / Shader；
///   * 渲染路径只消费 DrawItem，不认识骰子、地面这些具体类型。
/// 想加新的可绘制类型，只要实现 SceneObject 并产出 DrawItem 即可。
struct DrawItem
{
    /// 闭合网格在阴影 pass 里只画背面可消除 acne；单面片则保持正面。
    enum class FaceCull
    {
        Back,
        Front,
    };

    const Mesh* mesh = nullptr;
    glm::mat4 model{ 1.0f };
    PBRMaterial material;
    int pipCount = 0;          // 0 = 不画点数（骰子专用，其他物体保持 0）
    int firstIndex = 0;        // 只画一段索引；indexCount == 0 表示整网格
    int indexCount = 0;
    FaceCull shadowCull = FaceCull::Back;
};
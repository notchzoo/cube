#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "GltfLoader.h"

/// 一个控制面：几何、铰链、偏转限幅。角度单位弧度。
struct Su57SurfaceData
{
    GltfPartData geometry;
    glm::vec3 hingePoint{0.0f};
    glm::vec3 hingeAxis{0.0f, 0.0f, 1.0f};
    float minAngle = 0.0f;
    float maxAngle = 0.0f;
    int id = 0; // 1..10，见 Su57Rig.cpp 的命名表
};

struct Su57SplitData
{
    GltfPartData body;
    std::vector<Su57SurfaceData> surfaces;
};

/// 把单块 su_57.glb 按估算的铰链区域切成机身 + 10 个翼面。
///
/// 这是启发式切分：原模型是一整块焊接网格，没有独立翼面节点；切分边界是
/// 依据包围盒/平面估算的，偏转后会出现接缝，精度不等同于 DCC 里手工拆件。
Su57SplitData splitSu57(const GltfPartData& source);

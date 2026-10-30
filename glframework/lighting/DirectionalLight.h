#pragma once

#include <algorithm>

#include <glm/glm.hpp>

/// 方向光 = 传播方向 + 辐照度，没有物理位置。
/// 构造阴影矩阵时用到的“光源相机位置”只是实现手段，不代表灯光有位置。
struct DirectionalLight
{
    glm::vec3 direction{ -0.55f, -1.0f, -0.42f };  // 光线传播方向（世界空间）
    glm::vec3 color{ 1.0f, 0.96f, 0.89f };
    float intensity = 3.2f;

    glm::vec3 normalizedDirection() const
    {
        const float length = glm::length(direction);
        if (length < 1e-5f)
        {
            return glm::vec3(0.0f, -1.0f, 0.0f);
        }
        return direction / length;
    }

    /// 负强度没有物理意义，取 0 即关闭直射光。
    glm::vec3 radiance() const
    {
        return glm::max(color, glm::vec3(0.0f)) * std::max(intensity, 0.0f);
    }
};
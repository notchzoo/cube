#pragma once

#include <algorithm>

#include <glm/glm.hpp>

/// 金属度-粗糙度工作流参数。roughness 下限 0.04，避免 GGX 出现无限亮的针状高光。
struct PBRMaterial
{
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.28f;
    float ao = 1.0f;
    float opacity = 1.0f;

    /// 返回夹紧后的副本；不修改自身，便于 Application 在 setter 中保留原值语义。
    PBRMaterial clamped() const
    {
        PBRMaterial result = *this;
        result.albedo = glm::clamp(result.albedo, glm::vec3(0.0f), glm::vec3(1.0f));
        result.metallic = std::clamp(result.metallic, 0.0f, 1.0f);
        result.roughness = std::clamp(result.roughness, 0.04f, 1.0f);
        result.ao = std::clamp(result.ao, 0.0f, 1.0f);
        result.opacity = std::clamp(result.opacity, 0.0f, 1.0f);
        return result;
    }
};
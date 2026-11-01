#pragma once

#include <glm/glm.hpp>

#include "DirectionalLight.h"

/// 灯光设备：拥有方向光的参数，并负责构造它自己的光源正交矩阵。
///
/// 从 ShadowMap 里把"光照几何"拆出来之后：
///   * ShadowMap 只管深度纹理/帧缓冲，不再认识灯光；
///   * 太阳方向与阴影矩阵同源（sunDirection = -光传播方向），不会各算各的。
class LightRig
{
public:
    LightRig() = default;

    void setLight(const DirectionalLight& light) { m_light = light; }
    const DirectionalLight& light() const { return m_light; }

    /// 方向会归一化；零向量被忽略。
    void setDirection(const glm::vec3& direction);

    glm::vec3 normalizedDirection() const;

    /// 从场景指向太阳的单位向量（= 光传播方向取反），太阳图层用它定位。
    glm::vec3 sunDirection() const;

    /// 方向光的虚拟光源相机：正交投影。灯光没有物理位置。
    glm::mat4 lightSpaceMatrix() const;
    /// [阴影跟随] Application::updateVehicleCamera 在物理启用后调用，覆盖飞机及其地面投影。
    void setShadowFocus(glm::vec3 center, float halfExtent);

private:
    DirectionalLight m_light;
    glm::vec3 m_focus{0, 1, 0};
    float m_halfExtent = 9;
    bool m_tracking = false;
};

#include "LightRig.h"

#include <cmath>
#include <algorithm>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
// 阴影正交投影覆盖的场景范围，以及虚拟光源相机的摆放参数。
constexpr float kLightCameraDistance = 18.0f;
constexpr float kDepthNear = 15.0f;
constexpr float kDepthFar = 21.0f;
}

void LightRig::setDirection(const glm::vec3& direction)
{
    const float length = glm::length(direction);
    if (length < 1e-5f)
    {
        std::cerr << "[LightRig] light direction is almost zero, ignored\n";
        return;
    }
    m_light.direction = direction / length;
}

glm::vec3 LightRig::normalizedDirection() const
{
    return m_light.normalizedDirection();
}

glm::vec3 LightRig::sunDirection() const
{
    return -normalizedDirection();
}

void LightRig::setShadowFocus(glm::vec3 center, float halfExtent)
{
    m_focus = center;
    m_halfExtent = std::clamp(halfExtent, 9.0f, 500.0f);
    m_tracking = true;
}

glm::mat4 LightRig::lightSpaceMatrix() const
{
    const glm::vec3 direction = normalizedDirection();

    // 沿光线反向后退得到虚拟相机位置；它不代表灯光有实体位置。
    const float distance = m_tracking ? m_halfExtent * 2 : kLightCameraDistance;
    const glm::vec3 eye = m_focus - direction * distance;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(direction, up)) > 0.99f)
    {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::mat4 lightView = glm::lookAt(eye, m_focus, up);
    const glm::mat4 lightProjection =
        glm::ortho(-m_halfExtent, m_halfExtent, -m_halfExtent, m_halfExtent, m_tracking ? .5f : kDepthNear,
                   m_tracking ? distance * 2 + m_halfExtent : kDepthFar);
    return lightProjection * lightView;
}

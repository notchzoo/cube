#include "Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr float kPitchLimitDegrees = 89.0f;
constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;

// 滚轮升降的平滑时间常数（1/s）：越大越快贴到目标高度。
constexpr float kElevationSmoothing = 14.0f;
// 差小于这个值就直接吸附，避免无限逼近导致每帧都改一点。
constexpr float kElevationSnapDistance = 0.0005f;
}

void Camera::setYawPitch(float yawDegrees, float pitchDegrees)
{
    m_yaw = yawDegrees;
    m_pitch = std::clamp(pitchDegrees, -kPitchLimitDegrees, kPitchLimitDegrees);
    updateVectors();
}

void Camera::moveLocal(float forward, float right, float worldUp, float deltaSeconds)
{
    // 前后移动只用 yaw 决定的水平朝向，避免抬头时向前飞。
    const float yawRadians = m_yaw * kDegreesToRadians;
    const glm::vec3 horizontalForward(std::cos(yawRadians), 0.0f, std::sin(yawRadians));
    const glm::vec3 horizontalRight(-horizontalForward.z, 0.0f, horizontalForward.x);

    const float distance = m_moveSpeed * deltaSeconds;
    m_position += horizontalForward * (forward * distance);
    m_position += horizontalRight * (right * distance);

    // Page Up/Down 是连续输入，直接同时推进位置和目标，避免把正在进行的滚轮升降"吃掉"。
    const float verticalDelta = worldUp * distance;
    m_position.y += verticalDelta;
    m_elevationTarget += verticalDelta;
}

void Camera::updateElevation(float deltaSeconds)
{
    const float difference = m_elevationTarget - m_position.y;
    if (std::abs(difference) < kElevationSnapDistance)
    {
        m_position.y = m_elevationTarget;
        return;
    }

    // 指数平滑：1 - exp(-k*dt) 与帧率无关，等价于"每秒衰减到 1/e^k"。
    const float blend = 1.0f - std::exp(-kElevationSmoothing * deltaSeconds);
    m_position.y += difference * blend;
}

void Camera::processMouseMovement(float deltaX, float deltaY)
{
    m_yaw += deltaX * m_mouseSensitivity;
    m_pitch -= deltaY * m_mouseSensitivity;
    m_pitch = std::clamp(m_pitch, -kPitchLimitDegrees, kPitchLimitDegrees);
    updateVectors();
}

glm::mat4 Camera::viewMatrix() const
{
    return glm::lookAt(m_position, m_position + m_front, m_worldUp);
}

void Camera::updateVectors()
{
    const float yawRadians = m_yaw * kDegreesToRadians;
    const float pitchRadians = m_pitch * kDegreesToRadians;

    m_front.x = std::cos(pitchRadians) * std::cos(yawRadians);
    m_front.y = std::sin(pitchRadians);
    m_front.z = std::cos(pitchRadians) * std::sin(yawRadians);
    m_front = glm::normalize(m_front);

    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
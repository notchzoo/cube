#include "FixedWingAutopilot.h"

#include <algorithm>
#include <cmath>

namespace flight
{
namespace
{
constexpr float kRotateSpeed = 72.0f;      // m/s
constexpr float kTargetAltitude = 300.0f;  // m
constexpr float kTargetSpeed = 160.0f;     // m/s
constexpr float kCruiseBank = 0.314f;      // rad，约 18° 的缓慢盘旋
}

FixedWingControls FixedWingAutopilot::update(const RigidBodyState& state, float deltaSeconds, bool autoThrottle)
{
    FixedWingControls controls;
    const float speed = glm::length(state.velocity);

    if (state.grounded)
    {
        m_airborneTime = 0.0f;
        m_throttleIntegral = 0.0f;
        controls.throttle = autoThrottle ? 1.0f : 0.0f;
        controls.pitch = speed > kRotateSpeed ? 0.45f : 0.0f;
        return controls;
    }

    m_airborneTime += std::max(0.0f, deltaSeconds);

    const glm::vec3 forward = state.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 up = state.orientation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = state.orientation * glm::vec3(0.0f, 0.0f, 1.0f);

    const float pitchAngle = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
    const float bankAngle = std::atan2(-right.y, up.y);

    // 高度环给出目标爬升率，再转成目标俯仰角；迎角接近失速时不再继续拉杆。
    const float climbRate = std::clamp((kTargetAltitude - state.position.y) * 0.15f, -12.0f, 15.0f);
    float pitchTarget = std::clamp((climbRate - state.velocity.y) * 0.01f + 0.04f, -0.22f, 0.32f);

    const glm::vec3 velocityBody = glm::conjugate(state.orientation) * state.velocity;
    const float alpha = speed > 0.5f ? std::atan2(-velocityBody.y, velocityBody.x) : 0.0f;
    if (alpha > 0.24f)
        pitchTarget = std::min(pitchTarget, 0.0f);

    controls.pitch = std::clamp((pitchTarget - pitchAngle) * 3.0f, -1.0f, 1.0f);
    const float bankTarget = (state.position.y > 120.0f && speed > 90.0f) ? kCruiseBank : 0.0f;
    controls.roll = std::clamp((bankTarget - bankAngle) * 2.0f, -1.0f, 1.0f);
    controls.yaw = 0.0f;

    if (autoThrottle)
    {
        // PI 速度保持：纯 P 控制会留下稳态误差，导致“一直加速”。
        const float speedError = kTargetSpeed - speed;
        m_throttleIntegral = std::clamp(m_throttleIntegral + speedError * deltaSeconds * 0.006f, -0.40f, 0.50f);
        controls.throttle = std::clamp(0.35f + speedError * 0.008f + m_throttleIntegral, 0.0f, 1.0f);
    }
    else
    {
        m_throttleIntegral = 0.0f;
        controls.throttle = 0.0f;
    }
    return controls;
}
} // namespace flight

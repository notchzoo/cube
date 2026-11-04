#include "FixedWingObject.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

FixedWingObject::FixedWingObject(std::vector<Part> parts, const glm::mat4& visualFromBody,
                                 const flight::RigidBodyState& initialState)
    : m_parts(std::move(parts)), m_initialState(initialState), m_visualFromBody(visualFromBody)
{
    m_dynamics.reset(m_initialState);
    applyState(m_initialState);
}

void FixedWingObject::enableFlight()
{
    // 首次进入由构造函数 reset；退出后再进入从暂停处继续，不重置位置/速度。
    m_flightEnabled = true;
    m_manualThrottle = false;
    m_autoThrottleIntegral = 0.0f;
    applyState(m_dynamics.interpolatedState());
}

void FixedWingObject::setManualControls(const flight::FixedWingControls& controls)
{
    m_manualControls = controls;
}

void FixedWingObject::setThrottleControl(bool manual, float throttle)
{
    m_manualThrottle = manual;
    m_manualThrottleValue = std::clamp(throttle, 0.0f, 1.0f);
}

void FixedWingObject::setSurfaceTestAngle(int group, float radians)
{
    if (group < 0 || group > 4)
        return;
    m_surfaceTestOverride = true;
    const int first = group * 2;
    m_surfaceTestAngles[first] = radians;
    m_surfaceTestAngles[first + 1] = radians;
    m_surfaceAngles[first] = radians;
    m_surfaceAngles[first + 1] = radians;
}

void FixedWingObject::clearSurfaceTestAngles()
{
    m_surfaceTestOverride = false;
    m_surfaceTestAngles.fill(0.0f);
    m_surfaceAngles.fill(0.0f);
}

void FixedWingObject::setVisualSurfaceOverride(const std::array<float, 10>& angles)
{
    m_visualOverrideAngles = angles;
    m_visualSurfaceOverride = true;
}

void FixedWingObject::clearVisualSurfaceOverride()
{
    m_visualSurfaceOverride = false;
    m_visualOverrideAngles.fill(0.0f);
}

void FixedWingObject::setBrakes(bool enabled)
{
    m_dynamics.setBrakes(enabled);
}

void FixedWingObject::resetFlight()
{
    m_dynamics.reset(m_initialState);
    m_manualThrottle = true;
    m_manualThrottleValue = 0.0f;
    m_autoThrottleIntegral = 0.0f;
    m_manualControls = {};
    m_surfaceTestOverride = false;
    m_surfaceTestAngles.fill(0.0f);
    m_visualSurfaceOverride = false;
    m_visualOverrideAngles.fill(0.0f);
    m_surfaceAngles.fill(0.0f);
    m_renderState = m_initialState;
    applyState(m_renderState);
}

void FixedWingObject::update(float deltaSeconds)
{
    if (!m_flightEnabled || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        return;

    flight::FixedWingControls controls = m_manualControls;
    if (m_manualThrottle)
    {
        controls.throttle = m_manualThrottleValue;
    }
    else
    {
        // 自动油门：没有姿态自动驾驶，只保持约 160 m/s 的速度。
        const float speed = glm::length(m_dynamics.state().velocity);
        const float error = 160.0f - speed;
        m_autoThrottleIntegral = std::clamp(m_autoThrottleIntegral + error * 0.006f * deltaSeconds, -0.4f, 0.6f);
        controls.throttle = std::clamp(0.35f + error * 0.008f + m_autoThrottleIntegral, 0.0f, 1.0f);
    }

    m_dynamics.setControls(controls);
    m_dynamics.advance(deltaSeconds);

    const flight::ControlSurfaceAngles& surfaces = m_dynamics.controlSurfaces();
    m_surfaceAngles = {surfaces.flaperonRight, surfaces.flaperonLeft, surfaces.leadingEdgeFlap,
                       surfaces.leadingEdgeFlap, surfaces.stabilator,        surfaces.stabilator,
                       surfaces.rudder,        surfaces.rudder,              surfaces.nozzlePitch,
                       surfaces.nozzlePitch};
    // 翼面检查模式在飞行中也只覆盖视觉偏角；物理仍走 FBW 计算出的力矩。
    if (m_surfaceTestOverride)
        m_surfaceAngles = m_surfaceTestAngles;
    else if (m_visualSurfaceOverride)
        m_surfaceAngles = m_visualOverrideAngles;

    const flight::RigidBodyState visualState = m_dynamics.interpolatedState();
    const float agl = visualState.position.y - m_dynamics.parameters().gearHeight;
    if (!m_surfaceTestOverride && (visualState.grounded || agl < m_dynamics.parameters().landingAgl))
    {
        // 低空/地面时左右 flaperon 保持水平；K 翼面检查时允许显示检查偏角。
        m_surfaceAngles[0] = 0.0f;
        m_surfaceAngles[1] = 0.0f;
    }
    applyState(visualState);
}

void FixedWingObject::applyState(const flight::RigidBodyState& state)
{
    // 视觉与相机共用同一份插值状态：直接用未插值的物理状态做相机目标，
    // 120 Hz 的步进在高速下会表现为可感知的抖动。
    m_renderState = state;
    const glm::vec3 scenePosition = state.position * kUnitsPerMeter;
    m_model = glm::translate(glm::mat4(1.0f), scenePosition) * glm::mat4_cast(state.orientation) *
              m_visualFromBody;
}

void FixedWingObject::appendDrawItems(std::vector<DrawItem>& items) const
{
    for (const Part& part : m_parts)
    {
        if (!part.mesh || !part.mesh->valid())
            continue;

        DrawItem item;
        item.mesh = part.mesh.get();
        item.material = part.material;
        item.shadowCull = DrawItem::FaceCull::Front;

        glm::mat4 model = m_model * part.nodeTransform;
        if (part.surfaceIndex >= 0 && part.surfaceIndex < static_cast<int>(m_surfaceAngles.size()))
        {
            const float angle = std::clamp(m_surfaceAngles[part.surfaceIndex], part.minAngle, part.maxAngle);
            const glm::mat4 hinge = glm::translate(glm::mat4(1.0f), part.hingePoint) *
                                    glm::rotate(glm::mat4(1.0f), angle, part.hingeAxis) *
                                    glm::translate(glm::mat4(1.0f), -part.hingePoint);
            model *= hinge;
        }
        item.model = model;
        items.push_back(item);
    }
}

glm::vec3 FixedWingObject::focusPoint() const
{
    return m_renderState.position * kUnitsPerMeter;
}

glm::vec3 FixedWingObject::forwardDirection() const
{
    return m_renderState.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
}

float FixedWingObject::speedKmh() const
{
    return glm::length(m_renderState.velocity) * 3.6f;
}

float FixedWingObject::altitudeMeters() const
{
    return m_renderState.position.y;
}

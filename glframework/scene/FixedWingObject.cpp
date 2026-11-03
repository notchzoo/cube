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
    m_autopilotEnabled = true;
    m_manualThrottle = false;
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
    const int first = group * 2;
    m_surfaceAngles[first] = radians;
    m_surfaceAngles[first + 1] = radians;
}

void FixedWingObject::clearSurfaceTestAngles()
{
    m_surfaceAngles.fill(0.0f);
}

void FixedWingObject::setBrakes(bool enabled)
{
    m_dynamics.setBrakes(enabled);
}

void FixedWingObject::resetFlight()
{
    m_dynamics.reset(m_initialState);
    m_autopilot.reset();
    m_autopilotEnabled = false;
    m_manualThrottle = true;
    m_manualThrottleValue = 0.0f;
    m_manualControls = {};
    m_surfaceAngles.fill(0.0f);
    m_renderState = m_initialState;
    applyState(m_renderState);
}

void FixedWingObject::update(float deltaSeconds)
{
    if (!m_flightEnabled || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        return;

    flight::FixedWingControls controls =
        m_autopilot.update(m_dynamics.state(), deltaSeconds, !m_manualThrottle);
    if (!m_autopilotEnabled)
    {
        controls.pitch = m_manualControls.pitch;
        controls.roll = m_manualControls.roll;
        controls.yaw = m_manualControls.yaw;
    }
    if (m_manualThrottle)
        controls.throttle = m_manualThrottleValue;

    m_dynamics.setControls(controls);
    m_dynamics.advance(deltaSeconds);

    const flight::ControlSurfaceAngles& surfaces = m_dynamics.controlSurfaces();
    m_surfaceAngles = {surfaces.flaperonRight, surfaces.flaperonLeft, surfaces.leadingEdgeFlap,
                       surfaces.leadingEdgeFlap, surfaces.stabilator,        surfaces.stabilator,
                       surfaces.rudder,        surfaces.rudder,              surfaces.nozzlePitch,
                       surfaces.nozzlePitch};
    applyState(m_dynamics.interpolatedState());
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

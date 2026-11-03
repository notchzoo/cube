#include "FixedWingDynamics.h"

#include <algorithm>
#include <cmath>

namespace flight
{
namespace
{
bool finite(glm::vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

float sanitized(float v, float lo, float hi)
{
    return std::isfinite(v) ? std::clamp(v, lo, hi) : 0.0f;
}

glm::vec3 limited(glm::vec3 v, float maximum)
{
    const float length = glm::length(v);
    return length > maximum ? v * (maximum / length) : v;
}
} // namespace

FixedWingDynamics::FixedWingDynamics(const FixedWingParameters& parameters) : m_parameters(parameters)
{
    m_parameters.mass = std::max(1.0f, m_parameters.mass);
    m_parameters.inertia = glm::max(m_parameters.inertia, glm::vec3(1.0f));
    m_parameters.wingArea = std::max(0.1f, m_parameters.wingArea);
    m_parameters.airDensity = std::max(0.01f, m_parameters.airDensity);
    reset({});
}

void FixedWingDynamics::reset(const RigidBodyState& state)
{
    m_state = state;
    if (!finite(m_state.position))
        m_state.position = glm::vec3(0);
    if (!finite(m_state.velocity))
        m_state.velocity = glm::vec3(0);
    if (!finite(m_state.angularVelocity))
        m_state.angularVelocity = glm::vec3(0);

    const float qLength = glm::length(m_state.orientation);
    m_state.orientation =
        std::isfinite(qLength) && qLength > 0.001f ? glm::normalize(m_state.orientation) : glm::quat(1, 0, 0, 0);

    m_throttle = 0.0f;
    m_controls = {};
    m_angleOfAttack = 0.0f;
    m_loadFactor = 0.0f;
    m_sideslip = 0.0f;
    m_surfaceAngles = {};
    m_brakes = false;
    m_touchdownEvent = false;
    m_touchdownVerticalSpeed = 0.0f;
    m_accumulator = 0.0;
    resolveGround(0.0f);
    m_previous = m_state;
}

void FixedWingDynamics::setControls(const FixedWingControls& controls)
{
    m_controls.pitch = sanitized(controls.pitch, -1.0f, 1.0f);
    m_controls.roll = sanitized(controls.roll, -1.0f, 1.0f);
    m_controls.yaw = sanitized(controls.yaw, -1.0f, 1.0f);
    m_controls.throttle = sanitized(controls.throttle, 0.0f, 1.0f);
}

void FixedWingDynamics::advance(float deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        return;

    // 与直升机一致：帧时间只负责累积，物理按固定步长推进，避免帧率影响结果。
    m_accumulator += std::min(static_cast<double>(deltaSeconds), 0.25);
    while (m_accumulator + 1e-8 >= fixedStep)
    {
        m_previous = m_state;
        step(static_cast<float>(fixedStep));
        m_accumulator = std::max(0.0, m_accumulator - fixedStep);
    }
}

RigidBodyState FixedWingDynamics::interpolatedState() const
{
    if (m_state.grounded)
        return m_state;

    const float alpha = static_cast<float>(std::clamp(m_accumulator / fixedStep, 0.0, 1.0));
    RigidBodyState result = m_state;
    result.position = glm::mix(m_previous.position, m_state.position, alpha);
    result.orientation = glm::normalize(glm::slerp(m_previous.orientation, m_state.orientation, alpha));
    return result;
}

float FixedWingDynamics::airspeed() const
{
    return glm::length(m_state.velocity);
}

void FixedWingDynamics::step(float dt)
{
    const FixedWingParameters& p = m_parameters;

    // 油门响应；推力沿机体 +X，后续接矢量喷口时在这里叠加偏转分量。
    m_throttle += (m_controls.throttle - m_throttle) * (1.0f - std::exp(-p.thrustResponse * dt));

    const glm::quat q = m_state.orientation;
    const glm::vec3 velocityBody = glm::conjugate(q) * m_state.velocity;
    const float speed = glm::length(velocityBody);
    const float dynamicPressure = 0.5f * p.airDensity * speed * speed;

    float alpha = 0.0f;
    float beta = 0.0f;
    if (speed > 0.5f)
    {
        // 机体 +X 前、+Y 上：速度的 -Y 分量表示相对来流从下方来，迎角为正。
        alpha = std::atan2(-velocityBody.y, velocityBody.x);
        beta = std::atan2(velocityBody.z, std::max(0.1f, std::abs(velocityBody.x)));
    }
    m_angleOfAttack = alpha;
    m_sideslip = beta;

    float liftCoefficient = p.liftOffset + p.liftSlope * alpha;
    const bool stalled = std::abs(alpha) > p.stallAngle;
    if (stalled)
    {
        const float sign = alpha >= 0.0f ? 1.0f : -1.0f;
        const float over = std::min(1.0f, (std::abs(alpha) - p.stallAngle) / 0.25f);
        liftCoefficient = sign * p.maxLiftCoefficient * (1.0f - 0.45f * over);
    }
    liftCoefficient = std::clamp(liftCoefficient, -p.maxLiftCoefficient, p.maxLiftCoefficient);

    float dragCoefficient = p.dragCoefficient + p.inducedDragFactor * liftCoefficient * liftCoefficient;
    if (stalled)
        dragCoefficient += 0.08f;

    glm::vec3 forceBody(0.0f);
    if (speed > 0.5f)
    {
        const glm::vec3 velocityDirection = velocityBody / speed;
        glm::vec3 liftDirection = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), velocityDirection);
        const float liftLength = glm::length(liftDirection);
        liftDirection = liftLength > 1.0e-4f ? liftDirection / liftLength : glm::vec3(0.0f, 1.0f, 0.0f);

        // 过载硬限幅：正过载 < 12g，负过载 < 3g。升力被夹住后，
        // 继续拉杆只会把迎角推高，不会让结构过载继续增长。
        const float liftNewtons = std::clamp(liftCoefficient * dynamicPressure * p.wingArea,
                                             -p.maxNegativeLoadFactor * p.mass * p.gravity,
                                             p.maxPositiveLoadFactor * p.mass * p.gravity);
        m_loadFactor = liftNewtons / (p.mass * p.gravity);

        forceBody += liftDirection * liftNewtons;
        forceBody -= velocityDirection * (dragCoefficient * dynamicPressure * p.wingArea);
        forceBody.z -= p.sideForceSlope * beta * dynamicPressure * p.wingArea;
    }
    else
    {
        m_loadFactor = 0.0f;
    }
    forceBody.x += m_throttle * p.maxThrust;

    const glm::vec3 forceWorld = q * forceBody + glm::vec3(0.0f, -p.mass * p.gravity, 0.0f);
    m_state.velocity += forceWorld / p.mass * dt;

    const float speedAfter = glm::length(m_state.velocity);
    if (speedAfter > p.maxAirspeed)
        m_state.velocity *= p.maxAirspeed / speedAfter;
    m_state.position += m_state.velocity * dt;

    // 速率指令辅助飞控：目标角速度由驾驶输入给出，飞控把目标角加速度反解成翼面偏角，
    // 再用翼面力矩推动角速度积分；视觉偏角和气动力矩使用同一份 ControlSurfaceAngles。
    // 侧滑产生的风标偏航让机头跟随速度矢量，倾斜飞行时才能自然改变水平航向。
    const float weathervaneYawRate = std::clamp(-p.weathervaneGain * beta, -0.6f, 0.6f);
    const glm::vec3 targetRate(m_controls.roll * p.maxRollRate,
                               -m_controls.yaw * p.maxYawRate + weathervaneYawRate,
                               m_controls.pitch * p.maxPitchRate);
    const glm::vec3 rateError = targetRate - m_state.angularVelocity;
    const glm::vec3 desiredAcceleration = rateError * p.rateGain - m_state.angularVelocity * p.angularDamping;
    const glm::vec3 desiredMoment = p.inertia * desiredAcceleration;

    const float rollEffectiveness =
        2.0f * dynamicPressure * p.wingArea * p.wingSpan * p.aileronMomentCoefficient;
    const float yawEffectiveness = dynamicPressure * p.wingArea * p.wingSpan * p.rudderMomentCoefficient;
    const float pitchEffectiveness = dynamicPressure * p.wingArea * p.meanChord * p.stabilatorMomentCoefficient;

    const auto moveSurface = [&](float current, float target)
    {
        const float maxStep = p.surfaceAngularRate * dt;
        return current + std::clamp(target - current, -maxStep, maxStep);
    };

    const float aileronTarget = std::clamp(
        std::abs(rollEffectiveness) > 1.0e-3f ? desiredMoment.x / rollEffectiveness : 0.0f, -p.maxAileronAngle,
        p.maxAileronAngle);
    const float stabilatorTarget = std::clamp(
        std::abs(pitchEffectiveness) > 1.0e-3f ? desiredMoment.z / pitchEffectiveness : 0.0f,
        -p.maxStabilatorAngle, p.maxStabilatorAngle);
    const float rudderTarget = std::clamp(
        std::abs(yawEffectiveness) > 1.0e-3f ? desiredMoment.y / yawEffectiveness : 0.0f, -p.maxRudderAngle,
        p.maxRudderAngle);

    m_surfaceAngles.flaperonRight = moveSurface(m_surfaceAngles.flaperonRight, aileronTarget);
    m_surfaceAngles.flaperonLeft = -m_surfaceAngles.flaperonRight;
    m_surfaceAngles.stabilator = moveSurface(m_surfaceAngles.stabilator, stabilatorTarget);
    m_surfaceAngles.rudder = moveSurface(m_surfaceAngles.rudder, rudderTarget);

    // 前缘襟翼按迎角调度，喷口偏转暂时只做视觉；两者不参与当前力矩近似。
    const float leadingEdgeTarget = std::clamp((alpha - 0.08f) * 1.5f, 0.0f, 0.30f);
    m_surfaceAngles.leadingEdgeFlap = moveSurface(m_surfaceAngles.leadingEdgeFlap, leadingEdgeTarget);
    m_surfaceAngles.nozzlePitch = moveSurface(m_surfaceAngles.nozzlePitch,
                                              std::clamp(-m_controls.pitch * 0.2f, -0.2f, 0.2f));

    const float rollMoment = rollEffectiveness * m_surfaceAngles.flaperonRight;
    const float pitchMoment = pitchEffectiveness * m_surfaceAngles.stabilator;
    const float yawMoment = yawEffectiveness * m_surfaceAngles.rudder;
    m_state.angularVelocity +=
        glm::vec3(rollMoment / p.inertia.x, yawMoment / p.inertia.y, pitchMoment / p.inertia.z) * dt;
    m_state.angularVelocity = limited(m_state.angularVelocity, 4.0f);

    const float angularSpeed = glm::length(m_state.angularVelocity);
    if (angularSpeed > 1.0e-6f)
    {
        const glm::quat delta = glm::angleAxis(angularSpeed * dt, m_state.angularVelocity / angularSpeed);
        m_state.orientation = glm::normalize(m_state.orientation * delta);
    }

    resolveGround(dt);
}

void FixedWingDynamics::resolveGround(float dt)
{
    const float ground = m_parameters.gearHeight;
    const bool wasGrounded = m_state.grounded;
    const bool touching = m_state.position.y <= ground + 0.03f;
    if (!wasGrounded && touching && m_state.velocity.y < -0.2f)
    {
        m_touchdownEvent = true;
        m_touchdownVerticalSpeed = m_state.velocity.y;
    }
    m_state.grounded = touching;
    if (m_state.position.y < ground)
        m_state.position.y = ground;

    if (m_state.grounded)
    {
        if (m_state.velocity.y < 0.0f)
            m_state.velocity.y = 0.0f;

        // 刹车让滚动阻力明显增大；松开 B 后回到正常滚动阻力。
        const float frictionRate = m_brakes ? 4.0f : m_parameters.groundFriction;
        const float friction = std::exp(-frictionRate * dt);
        m_state.velocity.x *= friction;
        m_state.velocity.z *= friction;
        m_state.angularVelocity *= std::exp(-(m_brakes ? 10.0f : 6.0f) * dt);

        // 低速地面转向：刹车时不允许转向，避免停住后原地打转。
        const float groundSpeed = glm::length(glm::vec3(m_state.velocity.x, 0.0f, m_state.velocity.z));
        if (!m_brakes && groundSpeed > 2.0f)
        {
            const float targetYawRate = -m_controls.yaw * 0.45f;
            m_state.angularVelocity.y +=
                (targetYawRate - m_state.angularVelocity.y) * (1.0f - std::exp(-4.0f * dt));
        }
        if (m_brakes && groundSpeed < 0.5f)
        {
            m_state.velocity.x = 0.0f;
            m_state.velocity.z = 0.0f;
        }
    }
}
} // namespace flight

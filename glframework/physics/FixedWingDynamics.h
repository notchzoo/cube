#pragma once
#include "FixedWingControls.h"
#include "FixedWingParameters.h"
#include "RigidBodyState.h"

namespace flight
{
/// [固定翼物理] 当前翼面偏角（弧度）；视觉与气动力矩共用同一份状态。
struct ControlSurfaceAngles
{
    float flaperonLeft = 0.0f;
    float flaperonRight = 0.0f;
    float leadingEdgeFlap = 0.0f;
    float stabilator = 0.0f;
    float rudder = 0.0f;
    float nozzlePitch = 0.0f;
};

/// [固定翼物理] 6DOF 刚体 + 升力/阻力/侧力/推力/重力，120 Hz 固定步长。
///
/// 姿态层用速率指令辅助飞控（rate command）：飞控把目标角加速度反解成翼面偏角，
/// 再由翼面力矩推动角速度/姿态积分，不是直接写姿态。气动力由迎角、空速和动压计算，
/// 失速后 CL 下降、CD 上升。
class FixedWingDynamics
{
public:
    explicit FixedWingDynamics(const FixedWingParameters& parameters = {});

    void reset(const RigidBodyState& state);
    void setControls(const FixedWingControls& controls);
    void advance(float deltaSeconds);

    const RigidBodyState& state() const { return m_state; }
    RigidBodyState interpolatedState() const;
    const FixedWingParameters& parameters() const { return m_parameters; }
    const ControlSurfaceAngles& controlSurfaces() const { return m_surfaceAngles; }
    float throttle() const { return m_throttle; }
    float airspeed() const;
    float angleOfAttack() const { return m_angleOfAttack; }
    /// 机体上轴过载；1.0 表示升力等于重力，负值表示推杆产生的负过载。
    float loadFactor() const { return m_loadFactor; }
    float sideslip() const { return m_sideslip; }
    void setBrakes(bool enabled) { m_brakes = enabled; }
    bool brakes() const { return m_brakes; }
    /// 起落架/前轮逻辑状态；没有几何模型，只影响近地面减速和 HUD。
    void setGearDown(bool down) { m_gearDown = down; }
    bool gearDown() const { return m_gearDown; }
    /// 直接翼面模式：跳过速率指令反解，用驾驶给出的固定小偏角直接产生力矩。
    void setDirectSurfaceMode(bool enabled);
    bool directSurfaceMode() const { return m_directSurfaces; }
    void setDirectSurfaceTarget(const ControlSurfaceAngles& target) { m_directTarget = target; }
    float lastTouchdownVerticalSpeed() const { return m_touchdownVerticalSpeed; }
    /// 读取并清除“刚刚接地”事件，Application 每帧消费一次。
    bool consumeTouchdownEvent()
    {
        const bool event = m_touchdownEvent;
        m_touchdownEvent = false;
        return event;
    }

    static constexpr double fixedStep = 1.0 / 120.0;

private:
    void step(float dt);
    void resolveGround(float dt);

    FixedWingParameters m_parameters;
    RigidBodyState m_state;
    RigidBodyState m_previous;
    FixedWingControls m_controls;
    float m_throttle = 0.0f;
    float m_angleOfAttack = 0.0f;
    float m_loadFactor = 0.0f;
    float m_sideslip = 0.0f;
    ControlSurfaceAngles m_surfaceAngles;
    ControlSurfaceAngles m_directTarget;
    bool m_directSurfaces = false;
    bool m_brakes = false;
    bool m_gearDown = true;
    bool m_touchdownEvent = false;
    float m_touchdownVerticalSpeed = 0.0f;
    double m_accumulator = 0.0;
};
}

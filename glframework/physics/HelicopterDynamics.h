#pragma once
#include "RigidBodyState.h"
#include "FlightControls.h"
#include "HelicopterParameters.h"

namespace flight
{
/// [飞行物理] 独立 CPU 动力学，不依赖 GLFW/OpenGL。
/// 调用入口为 AttackHelicopterObject::update；内部固定 120 Hz，画面使用插值状态。
class HelicopterDynamics
{
public:
    explicit HelicopterDynamics(const HelicopterParameters& parameters={});
    /// 初始化/重置物理状态；会清空累计时间和驾驶指令，用于首次驾驶或外部重设 Model。
    void reset(const RigidBodyState& state,float rotorPower=1);
    /// 保存并限制驾驶指令，在下一次固定物理步生效；不在此处推进时间。
    void setControls(const FlightControls& controls);
    /// 每个渲染帧调用一次。累计秒数，再调用若干次 step(fixedStep)。
    void advance(float deltaSeconds);
    /// 最新物理状态，用于速度/高度显示和逻辑判断。
    const RigidBodyState& state() const { return m_state; }
    /// 画面专用：位置和姿态插值，避免渲染帧率与物理频率不同导致抖动。
    RigidBodyState interpolatedState() const;
    const HelicopterParameters& parameters() const { return m_parameters; }
    float rotorPower() const { return m_rotorPower; } // [0,1] 归一化转速，也驱动旋翼动画
    static constexpr double fixedStep=1.0/120.0;
private:
    void step(float dt);          // 飞控 -> 力/力矩 -> 角速度/姿态 -> 速度/位置 -> 地面接触
    void resolveGround(float dt); // 仅处理起落架与 Y=0 平面，不包含通用物体碰撞
    HelicopterParameters m_parameters;
    RigidBodyState m_state,m_previous;
    FlightControls m_controls;
    float m_rotorPower=1;
    double m_accumulator=0;
};
}

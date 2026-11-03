#pragma once
#include "FixedWingControls.h"
#include "RigidBodyState.h"

namespace flight
{
/// [固定翼辅助飞控] 起飞 + 爬升到巡航高度 + 保持高度/速度/轻微盘旋。
/// 只输出归一化驾驶指令，真正的运动仍由 FixedWingDynamics 的气动力和积分产生。
class FixedWingAutopilot
{
public:
    /// autoThrottle=false 时只计算姿态指令，油门交给驾驶者；积分项会清零，避免重新接管时残留。
    FixedWingControls update(const RigidBodyState& state, float deltaSeconds, bool autoThrottle);
    void reset()
    {
        m_airborneTime = 0.0f;
        m_throttleIntegral = 0.0f;
    }

private:
    float m_airborneTime = 0.0f;
    float m_throttleIntegral = 0.0f;
};
}

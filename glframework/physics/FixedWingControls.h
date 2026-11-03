#pragma once
#include <glm/glm.hpp>

namespace flight
{
/// [固定翼物理] 驾驶指令，全部是归一化输入。
/// 与直升机 FlightControls 分开：固定翼用俯仰/滚转/偏航角速度指令 + 油门。
struct FixedWingControls
{
    float pitch = 0.0f;    // [-1,1]，正值抬头
    float roll = 0.0f;     // [-1,1]，正值右滚
    float yaw = 0.0f;      // [-1,1]，正值机头右偏
    float throttle = 0.0f; // [0,1]
};
}

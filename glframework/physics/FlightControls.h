#pragma once
#include <glm/glm.hpp>

namespace flight
{
/// [飞行物理] Application::processInput 生成的驾驶指令。
/// 调用链：AttackHelicopterObject::setFlightControls -> update -> Dynamics::setControls。
/// 这些是辅助飞控的目标值，最终仍通过力与力矩积分产生运动，不直接修改飞机位置。
struct FlightControls
{
    glm::vec3 horizontalVelocity{0}; // 世界坐标目标速度，米/秒；由机体 WASD 方向换算，忽略 Y
    float climb=0;                   // [-1,1] 升降；0 表示目标垂直速度为零
    float heading=0;                 // 绕世界 +Y 的目标航向，弧度；只由 Q/E 改变
    float pedals=0;                  // [-1,1] 尾桨变距及偏航力矩；正值为 +Y 旋转
    float stabilatorTrim=0;          // [-1,1] 水平尾翼配平及轻微俯仰力矩
    bool engineOn=true;              // 发动机目标开关，实际转速在物理步中平滑变化
};
}

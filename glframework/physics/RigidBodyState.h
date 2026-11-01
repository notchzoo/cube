#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace flight
{
/// [飞行物理] 刚体状态。由 HelicopterDynamics::step 写入，场景对象读取后生成 Model。
struct RigidBodyState
{
    glm::vec3 position{0};             // 世界坐标，米；不是缩放后的渲染坐标
    glm::quat orientation{1, 0, 0, 0}; // 机体到世界的旋转；机头 +X，上方 +Y，右侧 +Z
    glm::vec3 velocity{0};             // 世界坐标线速度，米/秒
    glm::vec3 angularVelocity{0};      // 机体坐标角速度，弧度/秒
    bool grounded = false;             // 起落架与平地接触；落地时直接使用当前状态渲染
};
}

#include "ThirdPersonCamera.h"
#include "Camera.h"
#include <algorithm>
#include <cmath>

glm::vec3 ThirdPersonCamera::horizontalForward() const
{
    const float yaw=glm::radians(m_yaw);
    return {std::cos(yaw),0,std::sin(yaw)};
}
glm::vec3 ThirdPersonCamera::orbitForward() const
{
    const float pitch=glm::radians(m_pitch);
    return horizontalForward()*std::cos(pitch)+glm::vec3(0,std::sin(pitch),0);
}
glm::vec3 ThirdPersonCamera::movementDirection(float forward,float right) const
{
    const auto f=horizontalForward();
    auto movement=f*forward+glm::vec3(-f.z,0,f.x)*right;
    const float length=glm::length(movement);
    return length>1?movement/length:movement;
}
void ThirdPersonCamera::enter(glm::vec3 target,glm::vec3 forward,Camera& camera)
{
    // [第三人称相机] 仅进入驾驶时重设初始轨道；每帧 update 不重设 yaw，鼠标观察才不会被拉回。
    m_yaw=glm::degrees(std::atan2(forward.z,forward.x));m_pitch=-18;
    m_position=target-orbitForward()*m_distance;
    camera.setPosition(m_position);camera.setYawPitch(m_yaw,m_pitch);
}
void ThirdPersonCamera::mouseMovement(float dx,float dy)
{
    m_yaw=std::remainder(m_yaw+dx*.1f,360.0f);
    // [相机调参] 正/负 pitch 允许镜头到载具下方/上方；避开 ±90° 的观察方向奇点。
    m_pitch=std::clamp(m_pitch-dy*.1f,-85.0f,85.0f);
}
// [相机调参] 每格缩放 0.45，最近/最远为 3.5/12 场景单位。
void ThirdPersonCamera::zoom(float steps) { m_distance=std::clamp(m_distance-steps*.45f,3.5f,12.0f); }
void ThirdPersonCamera::update(glm::vec3 target,float dt,Camera& camera)
{
    auto desired=target-orbitForward()*m_distance;
    // [相机调参] 世界 Y 最低 0.35 防穿地；跟随指数系数 7 控制镜头追赶速度。
    desired.y=std::max(.35f,desired.y);
    m_position=glm::mix(m_position,desired,1-std::exp(-7*std::max(0.0f,dt)));
    const auto direction=glm::normalize(target-m_position);
    camera.setPosition(m_position);
    camera.setYawPitch(glm::degrees(std::atan2(direction.z,direction.x)),
                       glm::degrees(std::asin(std::clamp(direction.y,-1.0f,1.0f))));
}

#pragma once
#include <glm/glm.hpp>
class Camera;

/// [第三人称相机] 只管理观察轨道和跟随，不决定飞机朝向或 WASD 移动方向。
/// Application::setDriving 调用 enter；鼠标/滚轮回调修改轨道；每帧物理更新后调用 update。
class ThirdPersonCamera
{
public:
    void enter(glm::vec3 target, glm::vec3 aircraftForward, Camera& camera); // 进入驾驶，镜头初始置于机尾后上方
    void mouseMovement(float dx, float dy);                                  // 鼠标像素增量；只修改轨道 yaw/pitch
    void zoom(float steps);                                                  // 滚轮格数；修改跟随距离
    void setFollowGain(float gain) { m_followGain = gain > 0.0f ? gain : 7.0f; } // 高速目标需要更高的追赶系数
    void setHeadingFollow(bool enabled) { m_headingFollow = enabled; }
    /// 机体航向（度）；开启 heading follow 后镜头保持在机尾方向，鼠标输入变成相对偏移。
    void followHeading(float headingDegrees, float dt);
    void update(glm::vec3 target, float dt, Camera& camera);                 // 平滑跟随，并让 Camera 看向 target
    glm::vec3 horizontalForward() const;
    /// 观察方向的辅助换算；驾驶 WASD 使用 AttackHelicopterObject::movementDirection，勿混用。
    glm::vec3 movementDirection(float forward, float right) const;
    float distance() const { return m_distance; }

private:
    glm::vec3 orbitForward() const;
    float m_yaw = 0, m_pitch = -18, m_distance = 5.8f; // 角度制；默认俯角与跟随距离在这里调整
    float m_followGain = 7.0f;
    bool m_headingFollow = false;
    float m_yawOffset = 0.0f; // 鼠标相对机尾方向的偏移；heading follow 时保留
    glm::vec3 m_position{0};
};

#pragma once

#include <glm/glm.hpp>

/// 第一人称观察相机：只负责姿态与移动，不持有任何 GL 状态。
class Camera
{
public:
    Camera() = default;

    void setPosition(const glm::vec3& position)
    {
        m_position = position;
        m_elevationTarget = position.y;
    }
    void setYawPitch(float yawDegrees, float pitchDegrees);

    /// W/S 前后、A/D 左右：沿世界水平面移动（只受 yaw 影响，抬头不会起飞）。
    /// worldUp 对应 Page Up / Page Down 的世界 Y 升降。
    void moveLocal(float forward, float right, float worldUp, float deltaSeconds);

    /// 鼠标滚轮：只累加目标高度，不做瞬时位移，避免一格一跳。
    void addElevation(float deltaY) { m_elevationTarget += deltaY; }

    /// 每帧调用一次：把相机高度指数平滑地逼近目标高度（帧率无关）。
    void updateElevation(float deltaSeconds);

    /// 鼠标增量（像素）；pitch 被限制在 ±89°。
    void processMouseMovement(float deltaX, float deltaY);

    glm::mat4 viewMatrix() const;

    glm::vec3 position() const { return m_position; }
    glm::vec3 front() const { return m_front; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }

    void setMoveSpeed(float speed) { m_moveSpeed = speed; }
    float moveSpeed() const { return m_moveSpeed; }

private:
    void updateVectors();

    glm::vec3 m_position{0.0f, 2.0f, 7.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    float m_yaw = -90.0f; // 角度制对外，内部转弧度
    float m_pitch = -15.0f;
    float m_moveSpeed = 4.0f;
    float m_mouseSensitivity = 0.1f;

    // 滚轮目标高度：滚轮只改它，updateElevation() 每帧把 m_position.y 逼近它。
    float m_elevationTarget = m_position.y;
};
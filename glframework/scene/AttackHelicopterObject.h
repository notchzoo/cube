#pragma once

#include <array>
#include "AttackHelicopterGeometry.h"
#include "SceneObject.h"
#include "HelicopterDynamics.h"

/// [载具对象] 连接几何、物理和部件动画；所有 Mesh 的生命周期随 Scene 管理。
/// Application 创建对象并每帧调用 update，随后主画面/阴影 pass 只读取 DrawItem。
/// 静态网格只初始化上传一次，整机和部件运动都通过 Model 矩阵实现。
class AttackHelicopterObject : public SceneObject
{
public:
    AttackHelicopterObject(AttackHelicopterGeometry::Meshes meshes, const PBRMaterial& material,
                           const glm::mat4& model = glm::mat4(1));

    /// [载具更新] 每帧仅调用一次：物理积分 -> 整机 Model -> 旋翼相位/尾翼动画。
    void update(float deltaSeconds);
    /// 首次驾驶前是动画暂停开关；激活物理后是发动机开关，转速会逐渐变化。
    void setRotorsEnabled(bool enabled) { m_rotorsEnabled = enabled; }
    void toggleRotors() { m_rotorsEnabled = !m_rotorsEnabled; }
    bool rotorsEnabled() const { return m_rotorsEnabled; }
    /// [部件动画] 视觉转速，单位度/秒；不是经校准的实机转速。
    void setRotorSpeeds(float mainDegreesPerSecond, float tailDegreesPerSecond);
    void setRotorAngles(float mainDegrees, float tailDegrees); // 设置相位，截图复现入口，单位度
    void setPedalInput(float input);                           // 静态展示用，[-1,1] 尾桨变距；驾驶时由物理状态驱动
    void setStabilatorInput(float input);                      // 静态展示用，[-1,1] 水平尾翼偏转
    void setControlAngles(float tailPitchDegrees, float stabilatorDegrees); // 度制固定姿态，截图复现用
    // 查询接口统一返回弧度，与上面的度制设置接口区分。
    float mainRotorAngle() const { return m_mainAngle; }
    float tailRotorAngle() const { return m_tailAngle; }
    float tailPitchAngle() const { return m_tailPitch; }
    float stabilatorAngle() const { return m_stabilatorAngle; }
    /// 外部摆放整机；若物理已启用，会同步重置物理状态，避免渲染与物理脱节。
    void setModelMatrix(const glm::mat4& model);
    const glm::mat4& modelMatrix() const { return m_model; }
    /// [驾驶交互] 首次进入驾驶时初始化物理，再次调用不会重置飞机位置/速度。
    void enableFlightPhysics();
    bool flightPhysicsEnabled() const { return m_physicsEnabled; }
    void setFlightControls(const flight::FlightControls& controls)
    {
        m_flightControls = controls;
    }                           // 由 Application 每帧提交
    void clearFlightControls(); // 退出驾驶/自由观察时刹停悬停，保留当前航向及发动机开关
    const flight::HelicopterDynamics& dynamics() const { return m_dynamics; }
    glm::vec3 focusPoint() const;       // 第三人称注视点，随机体 Model 变化
    glm::vec3 forwardDirection() const; // 实际机头方向，不是目标航向，也不是摄像头方向
    /// [驾驶输入] 将 W/S、A/D 换算成机体相对的水平移动向量，斜向归一化，不控制高度。
    glm::vec3 movementDirection(float forward, float right) const;
    glm::vec3 interactionPoint() const;                  // [驾驶交互] 座舱锚点，返回世界坐标
    float interactionRadius() const;                     // 默认约 5 物理米，随 Model 缩放
    bool canEnterFrom(glm::vec3 observerPosition) const; // 三维距离判定，由 setDriving(true) 统一检查

    /// [部件动画] 顺序：机身、主旋翼、四片尾桨、水平尾翼；主画面和阴影共用。
    std::array<glm::mat4, 7> partTransforms() const;
    void appendDrawItems(std::vector<DrawItem>& items) const override;
    void appendShadowItems(std::vector<DrawItem>& items) const override;
    float opacity() const override { return m_material.opacity; }

private:
    AttackHelicopterGeometry::Meshes m_meshes;
    PBRMaterial m_material;
    glm::mat4 m_model{1};
    bool m_rotorsEnabled = true;
    float m_mainAngle = 0, m_tailAngle = 0;
    float m_mainSpeed = glm::radians(480.0f), m_tailSpeed = glm::radians(2080.0f);
    float m_pedalInput = 0, m_stabilatorInput = 0;
    float m_tailPitch = 0, m_stabilatorAngle = 0;
    flight::HelicopterDynamics m_dynamics;
    flight::FlightControls m_flightControls;
    bool m_physicsEnabled = false;
    float m_visualScale = .5f, m_worldUnitsPerMeter = .25f; // 默认每物理米 = 0.25 场景单位
    void resetPhysicsFromModel();
};

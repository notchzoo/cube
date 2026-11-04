#pragma once

#include <array>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "FixedWingControls.h"
#include "FixedWingDynamics.h"
#include "Mesh.h"
#include "PBRMaterial.h"
#include "RigidBodyState.h"
#include "SceneObject.h"

/// [固定翼对象] 连接拆分后的 glTF 视觉网格与 FixedWingDynamics。
///
/// 机身和控制面各是一个 Part；控制面带铰链点和轴，每帧从
/// FixedWingDynamics::controlSurfaces() 读取偏角并作用到 Model 矩阵。
class FixedWingObject : public SceneObject
{
public:
    static constexpr float kUnitsPerMeter = 0.25f; // 1 物理米 = 0.25 场景单位

    struct Part
    {
        std::shared_ptr<Mesh> mesh;
        PBRMaterial material;
        glm::mat4 nodeTransform{1.0f};
        glm::vec3 hingePoint{0.0f};
        glm::vec3 hingeAxis{0.0f, 0.0f, 1.0f};
        float minAngle = 0.0f;
        float maxAngle = 0.0f;
        int surfaceIndex = -1; // -1=机身；0..9 对应 ControlSurfaceAngles 的顺序
    };

    FixedWingObject(std::vector<Part> parts, const glm::mat4& visualFromBody,
                    const flight::RigidBodyState& initialState);

    /// 每帧仅调用一次：辅助飞控/手动输入 -> 物理积分 -> 视觉 Model。
    void update(float deltaSeconds);

    void enableFlight();
    bool flightEnabled() const { return m_flightEnabled; }
    /// 手动姿态会一直保持（速率指令目标为 0）；没有自动驾驶。
    void setManualControls(const flight::FixedWingControls& controls);
    /// manual=false 时由对象的 PI 自动油门保持巡航速度；manual=true 时保持给定油门。
    void setThrottleControl(bool manual, float throttle);
    void setBrakes(bool enabled);
    bool brakes() const { return m_dynamics.brakes(); }
    void setGearDown(bool down) { m_dynamics.setGearDown(down); }
    bool gearDown() const { return m_dynamics.gearDown(); }
    void setDirectSurfaceMode(bool enabled) { m_dynamics.setDirectSurfaceMode(enabled); }
    bool directSurfaceMode() const { return m_dynamics.directSurfaceMode(); }
    void setDirectSurfaceTarget(const flight::ControlSurfaceAngles& target) { m_dynamics.setDirectSurfaceTarget(target); }
    /// 回到初始停机状态：位置/姿态/速度/油门/舵面全部重置，保留视觉资源。
    void resetFlight();
    /// 翼面检查模式：0=flaperon 1=LEF 2=stabilator 3=rudder 4=nozzle；只在非飞行时使用。
    void setSurfaceTestAngle(int group, float radians);
    void clearSurfaceTestAngles();
    /// FBW 的纯视觉翼面覆盖：物理仍由 FBW 计算，只改显示角度。
    void setVisualSurfaceOverride(const std::array<float, 10>& angles);
    void clearVisualSurfaceOverride();

    void setCastsShadow(bool casts) { m_castsShadow = casts; }
    bool castsShadow() const override { return m_castsShadow; }
    float opacity() const override { return 1.0f; }

    void appendDrawItems(std::vector<DrawItem>& items) const override;
    void appendShadowItems(std::vector<DrawItem>& items) const override { appendDrawItems(items); }

    const flight::FixedWingDynamics& dynamics() const { return m_dynamics; }
    glm::vec3 initialPosition() const { return m_initialState.position; }
    bool consumeTouchdownEvent() { return m_dynamics.consumeTouchdownEvent(); }
    size_t partCount() const { return m_parts.size(); }
    glm::vec3 focusPoint() const;
    glm::vec3 forwardDirection() const;
    float speedKmh() const;
    float altitudeMeters() const;

private:
    void applyState(const flight::RigidBodyState& state);

    std::vector<Part> m_parts;
    flight::FixedWingDynamics m_dynamics;
    flight::RigidBodyState m_initialState;
    flight::RigidBodyState m_renderState; // 与视觉 Model 同一份插值状态，相机也读它，避免高速抖动
    glm::mat4 m_visualFromBody{1.0f};
    glm::mat4 m_model{1.0f};
    std::array<float, 10> m_surfaceAngles{};
    std::array<float, 10> m_surfaceTestAngles{};
    std::array<float, 10> m_visualOverrideAngles{};
    bool m_surfaceTestOverride = false;
    bool m_visualSurfaceOverride = false;
    bool m_flightEnabled = false;
    bool m_castsShadow = false;
    flight::FixedWingControls m_manualControls;
    bool m_manualThrottle = false;
    float m_manualThrottleValue = 0.0f;
    float m_autoThrottleIntegral = 0.0f;
};

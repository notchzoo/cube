#include "AttackHelicopterObject.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>
#include "Mesh.h"

namespace
{
float wrapAngle(double radians)
{
    constexpr double tau = 6.283185307179586;
    const double result = std::fmod(radians, tau);
    return static_cast<float>(result < 0 ? result + tau : result);
}
float finiteClamp(float value, float low, float high)
{
    return std::isfinite(value) ? std::clamp(value, low, high) : 0;
}
glm::mat4 translate(glm::vec3 point)
{
    return glm::translate(glm::mat4(1), point);
}
}

AttackHelicopterObject::AttackHelicopterObject(AttackHelicopterGeometry::Meshes meshes, const PBRMaterial& material,
                                               const glm::mat4& model)
    : m_meshes(std::move(meshes)),
      m_material(material.clamped()),
      m_model(model)
{
}

void AttackHelicopterObject::setRotorSpeeds(float mainDegrees, float tailDegrees)
{
    m_mainSpeed = glm::radians(finiteClamp(mainDegrees, -10000, 10000));
    m_tailSpeed = glm::radians(finiteClamp(tailDegrees, -10000, 10000));
}
void AttackHelicopterObject::setRotorAngles(float mainDegrees, float tailDegrees)
{
    if (std::isfinite(mainDegrees))
        m_mainAngle = wrapAngle(glm::radians(static_cast<double>(mainDegrees)));
    if (std::isfinite(tailDegrees))
        m_tailAngle = wrapAngle(glm::radians(static_cast<double>(tailDegrees)));
}
void AttackHelicopterObject::setPedalInput(float input)
{
    m_pedalInput = finiteClamp(input, -1, 1);
}
void AttackHelicopterObject::setStabilatorInput(float input)
{
    m_stabilatorInput = finiteClamp(input, -1, 1);
}
void AttackHelicopterObject::setControlAngles(float tailPitch, float stabilator)
{
    m_tailPitch = glm::radians(finiteClamp(tailPitch, -18, 18));
    m_stabilatorAngle = glm::radians(finiteClamp(stabilator, -20, 20));
    m_pedalInput = m_tailPitch / glm::radians(18.0f);
    m_stabilatorInput = m_stabilatorAngle / glm::radians(20.0f);
}

void AttackHelicopterObject::update(float dt)
{
    if (!std::isfinite(dt) || dt <= 0)
        return;
    // [载具更新] 驾驶物理一旦启用，退出驾驶后仍积分；未驾驶过则保持原来的静态展示。
    if (m_physicsEnabled)
    {
        m_flightControls.engineOn = m_rotorsEnabled;
        m_dynamics.setControls(m_flightControls);
        m_dynamics.advance(dt);
        const auto state = m_dynamics.interpolatedState();
        m_model = translate(state.position * m_worldUnitsPerMeter) * glm::mat4_cast(state.orientation) *
                  glm::scale(glm::mat4(1), glm::vec3(m_visualScale));
        m_pedalInput = std::clamp(m_flightControls.pedals + state.angularVelocity.y * .25f, -1.0f, 1.0f);
        const float speed = glm::dot(state.velocity, state.orientation * glm::vec3(1, 0, 0));
        m_stabilatorInput =
            std::clamp(m_flightControls.stabilatorTrim - speed / m_dynamics.parameters().maxSpeed * .35f, -1.0f, 1.0f);
    }
    // [部件动画] 激活物理后，发动机转速同时影响升力和视觉旋翼；静态展示仍支持立即暂停。
    const float rotorPower = m_physicsEnabled ? m_dynamics.rotorPower() : (m_rotorsEnabled ? 1.0f : 0.0f);
    if (rotorPower > 0)
    {
        m_mainAngle = wrapAngle(m_mainAngle + static_cast<double>(m_mainSpeed) * dt * rotorPower);
        m_tailAngle = wrapAngle(m_tailAngle + static_cast<double>(m_tailSpeed) * dt * rotorPower);
    }
    // Exponential response remains consistent when the frame rate changes.
    const float blend = 1 - std::exp(-8 * dt);
    m_tailPitch += (glm::radians(18.0f) * m_pedalInput - m_tailPitch) * blend;
    m_stabilatorAngle += (glm::radians(20.0f) * m_stabilatorInput - m_stabilatorAngle) * blend;
}

void AttackHelicopterObject::resetPhysicsFromModel()
{
    // [坐标换算] Model 的平移是场景单位；动力学内部全部使用米。
    // 默认模型缩放 .5、几何每单位 2 米，因此每物理米渲染为 .25 场景单位。
    m_visualScale = std::max(.001f, glm::length(glm::vec3(m_model[0])));
    // Geometry has two real metres per local unit; preserve the exhibit's scale.
    m_worldUnitsPerMeter = m_visualScale / 2;
    flight::RigidBodyState state;
    state.position = glm::vec3(m_model[3]) / m_worldUnitsPerMeter;
    state.orientation = glm::normalize(glm::quat_cast(glm::mat3(m_model) / m_visualScale));
    m_dynamics.reset(state, m_rotorsEnabled ? 1.0f : 0.0f);
    clearFlightControls();
}
void AttackHelicopterObject::setModelMatrix(const glm::mat4& model)
{
    m_model = model;
    if (m_physicsEnabled)
        resetPhysicsFromModel();
}
void AttackHelicopterObject::enableFlightPhysics()
{
    if (m_physicsEnabled)
        return;
    resetPhysicsFromModel();
    m_physicsEnabled = true;
}
void AttackHelicopterObject::clearFlightControls()
{
    m_flightControls = {};
    const auto forward = m_dynamics.state().orientation * glm::vec3(1, 0, 0);
    m_flightControls.heading = std::atan2(-forward.z, forward.x);
    m_flightControls.engineOn = m_rotorsEnabled;
}
glm::vec3 AttackHelicopterObject::focusPoint() const
{
    return glm::vec3(m_model * glm::vec4(-1, .5f, 0, 1));
}
glm::vec3 AttackHelicopterObject::forwardDirection() const
{
    return glm::normalize(glm::vec3(m_model[0]));
}
glm::vec3 AttackHelicopterObject::movementDirection(float forward, float right) const
{
    // [驾驶输入] 用实际机头的水平投影，而不是鼠标朝向，也不是尚未转到的目标航向。
    // 极端竖直姿态时改用机身侧轴推导前向，避免零长度归一化。
    auto f = glm::vec3(m_model[0]);
    f.y = 0;
    if (glm::dot(f, f) < .000001f)
    {
        const auto side = glm::vec3(m_model[2]);
        f = {side.z, 0, -side.x};
    }
    f = glm::normalize(f);
    auto movement = f * forward + glm::vec3(-f.z, 0, f.x) * right;
    const float length = glm::length(movement);
    return length > 1 ? movement / length : movement;
}
glm::vec3 AttackHelicopterObject::interactionPoint() const
{
    // [交互调参] 改这里可调整上车锚点；它是机体局部坐标，会随飞机的 Model 变换。
    return glm::vec3(m_model * glm::vec4(.65f, .50f, 0, 1)); // cockpit, not the tail / rotor bounds
}
float AttackHelicopterObject::interactionRadius() const
{
    // [交互调参] 2.5 个几何单位 = 5 物理米；默认 .5 展示缩放下是 1.25 场景单位。
    return 2.5f * glm::length(glm::vec3(m_model[0])); // five physical metres, scales with the exhibit
}
bool AttackHelicopterObject::canEnterFrom(glm::vec3 observerPosition) const
{
    // Full 3-D distance prevents boarding an aircraft flying above the observer.
    return glm::distance(observerPosition, interactionPoint()) <= interactionRadius();
}

std::array<glm::mat4, 7> AttackHelicopterObject::partTransforms() const
{
    // [部件动画] 子网格已以转轴为原点：父 Model × 转轴平移 × 部件旋转。
    // 尾桨先绕自己的径向轴变距，再绕尾轴公转；顺序不能交换。
    using namespace AttackHelicopterGeometry;
    std::array<glm::mat4, 7> transforms;
    transforms[0] = m_model;
    transforms[1] = m_model * translate(mainRotorPivot()) * glm::rotate(glm::mat4(1), m_mainAngle, glm::vec3(0, 1, 0));
    for (int i = 0; i < 4; ++i)
        transforms[i + 2] = m_model * translate(tailRotorPivot()) *
                            glm::rotate(glm::mat4(1), m_tailAngle + tailBladeRestAngle(i), glm::vec3(0, 0, 1)) *
                            glm::rotate(glm::mat4(1), m_tailPitch, glm::vec3(1, 0, 0));
    transforms[6] =
        m_model * translate(stabilatorPivot()) * glm::rotate(glm::mat4(1), m_stabilatorAngle, stabilatorAxis());
    return transforms;
}

void AttackHelicopterObject::appendDrawItems(std::vector<DrawItem>& items) const
{
    const Mesh* meshes[] = {m_meshes.body.get(),      m_meshes.mainRotor.get(), m_meshes.tailBlade.get(),
                            m_meshes.tailBlade.get(), m_meshes.tailBlade.get(), m_meshes.tailBlade.get(),
                            m_meshes.stabilator.get()};
    const auto transforms = partTransforms();
    for (size_t i = 0; i < transforms.size(); ++i)
    {
        if (!meshes[i] || !meshes[i]->valid())
            continue;
        DrawItem item;
        item.mesh = meshes[i];
        item.model = transforms[i];
        item.material = m_material;
        item.shadowCull = DrawItem::FaceCull::Front;
        items.push_back(item);
    }
}
void AttackHelicopterObject::appendShadowItems(std::vector<DrawItem>& items) const
{
    // [阴影同步] 复用主画面的变换，不在阴影 pass 更新动画相位或物理状态。
    appendDrawItems(items);
}

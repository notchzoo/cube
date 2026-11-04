#pragma once
#include <glm/glm.hpp>

namespace flight
{
/// [固定翼物理] 演示级参数：公开资料/估算值，不是 Su-57 实机标定数据。
///
/// 机体坐标沿用 RigidBodyState：+X 机头、+Y 向上、+Z 右侧；单位全部是 SI。
struct FixedWingParameters
{
    float mass = 18000.0f;                              // kg，接近空重
    glm::vec3 inertia{30000.0f, 120000.0f, 150000.0f};  // kg·m²；x=滚转 y=偏航 z=俯仰

    float wingArea = 78.8f;      // m²
    float wingSpan = 14.1f;      // m
    float meanChord = 5.6f;      // m，俯仰力矩力臂
    float airDensity = 1.225f;   // kg/m³，海平面
    float liftSlope = 3.5f;      // CL/rad
    float liftOffset = 0.06f;    // 零迎角升力系数
    float stallAngle = 0.28f;    // rad，约 16°
    float maxLiftCoefficient = 1.3f;
    float dragCoefficient = 0.022f;
    float inducedDragFactor = 0.12f;
    float sideForceSlope = 0.6f;

    // 结构/生理过载边界：正过载 < 12g，负过载 < 3g（演示用硬限幅）。
    float maxPositiveLoadFactor = 12.0f;
    float maxNegativeLoadFactor = 3.0f;
    // 垂尾风标效应：侧滑产生偏航角速度，让机头跟随速度矢量，倾斜后才能自然转弯。
    float weathervaneGain = 1.8f;

    // 控制面力矩系数（演示值）与偏转限幅；符号按“视觉正角 = 绕 +Z 后缘上偏”定义。
    float stabilatorMomentCoefficient = -0.35f; // 后缘上偏 -> 低头
    float aileronMomentCoefficient = 0.06f;     // 右副翼上偏 -> 右滚
    float rudderMomentCoefficient = -0.06f;     // 方向舵后缘左偏 -> 机头右偏
    float maxStabilatorAngle = 0.50f;           // rad，约 28°
    float maxAileronAngle = 0.40f;              // rad，约 23°
    float maxRudderAngle = 0.50f;               // rad，约 28°
    float surfaceAngularRate = 4.0f;            // rad/s，舵机速率限制

    float maxThrust = 294000.0f; // N，2 × 147 kN
    float thrustResponse = 1.8f; // 1/s，油门响应
    float maxAirspeed = 420.0f;  // m/s，仅用于防止数值爆炸

    float maxPitchRate = 0.35f;  // rad/s，约 20°/s；演示手感，不是实机飞控限幅
    float maxRollRate = 1.1f;    // rad/s，约 63°/s
    float maxYawRate = 0.3f;     // rad/s，约 17°/s
    float rateGain = 7.0f;       // 角速度跟踪增益
    float angularDamping = 0.8f;

    float gearHeight = 1.4f;      // m，重心到地面的高度
    float landingAgl = 3.2f;      // m，低空自动改平/起落架减速触发离地高度
    float takeoffSpeed = 70.0f;   // m/s，低于此速度地面不允许抬轮
    float groundFriction = 0.02f; // 1/s，滚动阻力；不是刹车
    float gravity = 9.81f;
};
}

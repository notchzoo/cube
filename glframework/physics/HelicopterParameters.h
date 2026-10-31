#pragma once
#include <array>
#include <glm/glm.hpp>

namespace flight
{
/// [飞行物理] 集中调参入口。HelicopterDynamics 构造时复制，运行中通过 parameters() 只读查询。
/// 全部使用 SI 单位，属于演示调参，不是经校准的 AH-64 实机数据。
struct HelicopterParameters
{
    float mass=8000;                          // 质量，kg
    glm::vec3 inertia{18000,100000,100000};     // 机体主轴转动惯量，kg·m²

    float gravity=9.81f;                      // 重力加速度，m/s²
    float thrustToWeight=1.9f;                // 满转速时最大推重比
    float maxSpeed=240;                        // 常规目标水平速度，m/s；Shift 提高目标值
    float climbRate=6;                        // 满升降输入的目标垂直速度，m/s
    float maxTilt=glm::radians(25.0f);         // 辅助飞控的最大倾角，弧度
    //float linearDrag=.10f,quadraticDrag=.008f; // 速度阻尼，决定减速及高速阻力
    float linearDrag = .0010f, quadraticDrag = .0008f;
    float velocityGain=1.4f,verticalGain=2.6f; // 水平及垂直目标速度的响应增益
    float attitudeGain=12,angularDamping=6;   // 姿态响应与角速度阻尼
    float spoolResponse=1.5f;                 // 发动机加减速响应，1/s
    float groundFriction=5;                   // 落地后水平速度衰减，1/s
    // 机体坐标的左/右主轮与尾轮最低点，单位米；已包含轮胎半径。
    std::array<glm::vec3,3> landingPoints{{
        {.70f,-1.6352f,1.015f},{.70f,-1.6352f,-1.015f},{-10.16f,-1.40f,0}}};
};
}

#include "HelicopterDynamics.h"
#include <algorithm>
#include <cmath>

namespace flight
{
namespace
{
bool finite(glm::vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
float sanitized(float v,float lo,float hi) { return std::isfinite(v)?std::clamp(v,lo,hi):0; }
glm::vec3 limited(glm::vec3 v,float maximum)
{
    const float length=glm::length(v);
    return length>maximum?v*(maximum/length):v;
}
}

HelicopterDynamics::HelicopterDynamics(const HelicopterParameters& parameters):m_parameters(parameters)
{
    m_parameters.mass=std::max(1.0f,m_parameters.mass);
    m_parameters.inertia=glm::max(m_parameters.inertia,glm::vec3(1));
    reset({});
}
void HelicopterDynamics::reset(const RigidBodyState& state,float rotorPower)
{
    m_state=state;
    if(!finite(m_state.position)) m_state.position=glm::vec3(0);
    if(!finite(m_state.velocity)) m_state.velocity=glm::vec3(0);
    if(!finite(m_state.angularVelocity)) m_state.angularVelocity=glm::vec3(0);
    const float qLength=glm::length(m_state.orientation);
    m_state.orientation=std::isfinite(qLength)&&qLength>.001f?glm::normalize(m_state.orientation):glm::quat(1,0,0,0);
    m_rotorPower=sanitized(rotorPower,0,1);
    m_accumulator=0;
    m_controls={};
    const auto forward=m_state.orientation*glm::vec3(1,0,0);
    m_controls.heading=std::atan2(-forward.z,forward.x);
    resolveGround(0);
    m_previous=m_state;
}
void HelicopterDynamics::setControls(const FlightControls& controls)
{
    m_controls=controls;
    if(!finite(m_controls.horizontalVelocity)) m_controls.horizontalVelocity=glm::vec3(0);
    m_controls.horizontalVelocity.y=0;
    m_controls.horizontalVelocity=limited(m_controls.horizontalVelocity,m_parameters.maxSpeed*1.6f);
    m_controls.climb=sanitized(m_controls.climb,-1,1);
    m_controls.pedals=sanitized(m_controls.pedals,-1,1);
    m_controls.stabilatorTrim=sanitized(m_controls.stabilatorTrim,-1,1);
    m_controls.heading=sanitized(m_controls.heading,-100000,100000);
}
void HelicopterDynamics::advance(float deltaSeconds)
{
    // [飞行物理] 外层按帧调用，内层固定步长积分；保留不足一个物理步的时间用于下一帧和插值。
    if(!std::isfinite(deltaSeconds)||deltaSeconds<=0) return;
    // Bound catch-up work following breakpoints/window stalls.
    m_accumulator+=std::min(static_cast<double>(deltaSeconds),.25);
    while(m_accumulator+1e-8>=fixedStep)
    {
        m_previous=m_state;
        step(static_cast<float>(fixedStep));
        m_accumulator=std::max(0.0,m_accumulator-fixedStep);
    }
}
RigidBodyState HelicopterDynamics::interpolatedState() const
{
    if(m_state.grounded) return m_state;
    const float alpha=static_cast<float>(std::clamp(m_accumulator/fixedStep,0.0,1.0));
    RigidBodyState result=m_state;
    result.position=glm::mix(m_previous.position,m_state.position,alpha);
    result.orientation=glm::normalize(glm::slerp(m_previous.orientation,m_state.orientation,alpha));
    return result;
}
void HelicopterDynamics::step(float dt)
{
    // [飞行物理] 更新顺序：转速 -> 辅助姿态目标 -> 力矩/角速度 -> 推力/位移 -> 地面接触。
    const auto& p=m_parameters;
    const float blend=1-std::exp(-p.spoolResponse*dt);
    m_rotorPower+=((m_controls.engineOn?1.0f:0.0f)-m_rotorPower)*blend;
    if(m_rotorPower<.0001f) m_rotorPower=0;

    // Assisted cyclic: aim the rotor thrust vector into the requested horizontal
    // acceleration. Actual translation still comes from thrust, gravity and drag.
    const glm::vec3 horizontal(m_state.velocity.x,0,m_state.velocity.z);
    const auto acceleration=limited((m_controls.horizontalVelocity-horizontal)*p.velocityGain,
                                    p.gravity*std::tan(p.maxTilt));
    const auto desiredUp=glm::normalize(glm::vec3(acceleration.x,p.gravity,acceleration.z));
    glm::vec3 desiredX(std::cos(m_controls.heading),0,-std::sin(m_controls.heading));
    desiredX=glm::normalize(desiredX-desiredUp*glm::dot(desiredX,desiredUp));
    const auto desiredZ=glm::normalize(glm::cross(desiredX,desiredUp));
    const auto desired=glm::quat_cast(glm::mat3(desiredX,desiredUp,desiredZ));
    auto error=glm::conjugate(m_state.orientation)*desired;
    if(error.w<0) error=-error;
    const glm::vec3 rotationError(error.x,error.y,error.z);
    const auto angularAcceleration=2.0f*p.attitudeGain*rotationError-p.angularDamping*m_state.angularVelocity+
                                   glm::vec3(0,m_controls.pedals*1.5f,m_controls.stabilatorTrim*.75f);
    const auto angularMomentum=p.inertia*m_state.angularVelocity;
    const auto gyro=glm::cross(m_state.angularVelocity,angularMomentum);
    // The assisted controller compensates gyroscopic coupling in its torque demand.
    const auto torque=(p.inertia*angularAcceleration+gyro)*m_rotorPower*m_rotorPower;
    m_state.angularVelocity+=(torque-gyro)/p.inertia*dt;
    m_state.angularVelocity*=std::exp(-.15f*dt);
    m_state.orientation=glm::normalize(m_state.orientation+
        .5f*(m_state.orientation*glm::quat(0,m_state.angularVelocity.x,m_state.angularVelocity.y,m_state.angularVelocity.z))*dt);

    // 推力沿机体上轴，而不是固定世界 +Y；倾斜后才能产生水平加速度。
    const auto rotorUp=m_state.orientation*glm::vec3(0,1,0);
    const float verticalAcceleration=p.gravity+p.verticalGain*(m_controls.climb*p.climbRate-m_state.velocity.y);
    const float thrust=p.mass*std::clamp(verticalAcceleration/std::max(.4f,rotorUp.y),0.0f,p.gravity*p.thrustToWeight)*
                       m_rotorPower*m_rotorPower;
    const auto drag=-p.mass*(p.linearDrag+p.quadraticDrag*glm::length(m_state.velocity))*m_state.velocity;
    const auto force=rotorUp*thrust+glm::vec3(0,-p.mass*p.gravity,0)+drag;
    m_state.velocity+=force/p.mass*dt;
    m_state.position+=m_state.velocity*dt; // semi-implicit Euler
    resolveGround(dt);
}
void HelicopterDynamics::resolveGround(float dt)
{
    // [飞行物理] 接触点包含轮胎半径；以最低点修正机体位置，不把整机原点压到 Y=0。
    float lowest=100000;
    for(auto local:m_parameters.landingPoints)
        lowest=std::min(lowest,(m_state.position+m_state.orientation*local).y);
    m_state.grounded=lowest<=.0001f;
    if(lowest<0)
    {
        m_state.position.y-=lowest;
        m_state.velocity.y=std::max(0.0f,m_state.velocity.y);
        const float friction=std::exp(-m_parameters.groundFriction*dt);
        m_state.velocity.x*=friction;m_state.velocity.z*=friction;
        m_state.angularVelocity*=std::exp(-10*dt);
    }
}
}

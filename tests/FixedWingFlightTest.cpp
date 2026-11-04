#include "FixedWingAutopilot.h"
#include "FixedWingDynamics.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
bool finiteVec(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float pitchOf(const flight::RigidBodyState& state)
{
    const glm::vec3 forward = state.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
    return std::asin(std::clamp(forward.y, -1.0f, 1.0f));
}

float headingOf(const flight::RigidBodyState& state)
{
    return std::atan2(state.velocity.z, state.velocity.x);
}
} // namespace

int main()
{
    flight::FixedWingDynamics dynamics;
    flight::FixedWingAutopilot autopilot;

    flight::RigidBodyState initial;
    initial.position = glm::vec3(0.0f, dynamics.parameters().gearHeight, 0.0f);
    initial.orientation = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    dynamics.reset(initial);

    float maximumAltitude = 0.0f;
    float maximumSpeed = 0.0f;
    for (int step = 0; step < 60 * 120; ++step)
    {
        const flight::FixedWingControls controls = autopilot.update(dynamics.state(), 1.0f / 120.0f, true);
        dynamics.setControls(controls);
        dynamics.advance(1.0f / 120.0f);

        const flight::RigidBodyState& state = dynamics.state();
        if (!finiteVec(state.position) || !finiteVec(state.velocity) || !std::isfinite(state.orientation.w))
        {
            std::cerr << "[FixedWingFlightTest] non-finite state at step " << step << '\n';
            return 1;
        }
        maximumAltitude = std::max(maximumAltitude, state.position.y);
        maximumSpeed = std::max(maximumSpeed, glm::length(state.velocity));
    }

    const flight::RigidBodyState& state = dynamics.state();
    const glm::vec3 forward = state.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
    const float forwardSpeed = glm::dot(forward, state.velocity);
    const float speed = glm::length(state.velocity);

    std::cout << "[FixedWingFlightTest] altitude=" << state.position.y << " m speed=" << speed
              << " m/s alpha=" << dynamics.angleOfAttack() << " grounded=" << state.grounded
              << " maxAltitude=" << maximumAltitude << " maxSpeed=" << maximumSpeed << '\n';

    if (state.grounded)
    {
        std::cerr << "[FixedWingFlightTest] aircraft never took off\n";
        return 1;
    }
    if (state.position.y < 150.0f || speed < 80.0f || forwardSpeed <= 0.0f)
    {
        std::cerr << "[FixedWingFlightTest] aircraft did not reach a stable forward flight state\n";
        return 1;
    }

    // 过载限幅：200 m/s 平飞，满拉杆 5 秒、满推杆 3 秒，检查升力硬限幅。
    flight::FixedWingDynamics limits;
    flight::RigidBodyState level;
    level.position = glm::vec3(0.0f, 1500.0f, 0.0f);
    level.velocity = glm::vec3(400.0f, 0.0f, 0.0f);
    level.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    limits.reset(level);

    float maximumLoadFactor = 0.0f;
    float minimumLoadFactor = 0.0f;
    for (int i = 0; i < 5 * 120; ++i)
    {
        flight::FixedWingControls controls;
        controls.pitch = 1.0f;
        controls.throttle = 1.0f;
        limits.setControls(controls);
        limits.advance(1.0f / 120.0f);
        maximumLoadFactor = std::max(maximumLoadFactor, limits.loadFactor());
        minimumLoadFactor = std::min(minimumLoadFactor, limits.loadFactor());
    }
    for (int i = 0; i < 3 * 120; ++i)
    {
        flight::FixedWingControls controls;
        controls.pitch = -1.0f;
        controls.throttle = 1.0f;
        limits.setControls(controls);
        limits.advance(1.0f / 120.0f);
        maximumLoadFactor = std::max(maximumLoadFactor, limits.loadFactor());
        minimumLoadFactor = std::min(minimumLoadFactor, limits.loadFactor());
    }

    std::cout << "[FixedWingFlightTest] load factor range: " << minimumLoadFactor << " g .. " << maximumLoadFactor
              << " g\n";
    if (maximumLoadFactor > 12.05f || minimumLoadFactor < -3.05f)
    {
        std::cerr << "[FixedWingFlightTest] load factor limit exceeded\n";
        return 1;
    }

    // 姿态保持：输入回中后俯仰角应保持，不应自己漂移或被自驾拉回。
    flight::FixedWingDynamics hold;
    flight::RigidBodyState levelState;
    levelState.position = glm::vec3(0.0f, 800.0f, 0.0f);
    levelState.velocity = glm::vec3(200.0f, 0.0f, 0.0f);
    hold.reset(levelState);

    flight::FixedWingControls holdControls;
    holdControls.pitch = 0.3f;
    holdControls.throttle = 0.5f;
    for (int i = 0; i < 120; ++i)
    {
        hold.setControls(holdControls);
        hold.advance(1.0f / 120.0f);
    }
    const float pitchAtRelease = pitchOf(hold.state());
    holdControls.pitch = 0.0f;
    for (int i = 0; i < 240; ++i)
    {
        hold.setControls(holdControls);
        hold.advance(1.0f / 120.0f);
    }
    const float pitchAfterHold = pitchOf(hold.state());
    std::cout << "[FixedWingFlightTest] pitch hold: " << pitchAtRelease * 180.0f / glm::pi<float>() << " deg -> "
              << pitchAfterHold * 180.0f / glm::pi<float>() << " deg\n";
    if (std::fabs(pitchAfterHold - pitchAtRelease) > 0.05f)
    {
        std::cerr << "[FixedWingFlightTest] pitch attitude is not held\n";
        return 1;
    }

    // 倾斜转弯：压坡度后回中，升力水平分量应改变速度方向。
    flight::FixedWingDynamics turn;
    turn.reset(levelState);
    const float initialHeading = headingOf(turn.state());
    flight::FixedWingControls turnControls;
    turnControls.roll = 0.5f;
    turnControls.throttle = 0.6f;
    for (int i = 0; i < 72; ++i)
    {
        turn.setControls(turnControls);
        turn.advance(1.0f / 120.0f);
    }
    turnControls.roll = 0.0f;
    for (int i = 0; i < 720; ++i)
    {
        turn.setControls(turnControls);
        turn.advance(1.0f / 120.0f);
    }
    const float headingChange =
        std::remainder(headingOf(turn.state()) - initialHeading, 2.0f * glm::pi<float>()) * 180.0f / glm::pi<float>();
    std::cout << "[FixedWingFlightTest] banked turn heading change: " << headingChange << " deg\n";
    if (std::fabs(headingChange) < 2.0f)
    {
        std::cerr << "[FixedWingFlightTest] banked turn did not change heading\n";
        return 1;
    }

    // 接地与刹车：低速进场后应产生 touchdown 事件，刹车能把滑跑速度降下来。
    flight::FixedWingDynamics landing;
    flight::RigidBodyState approach;
    approach.position = glm::vec3(0.0f, 80.0f, 0.0f);
    approach.velocity = glm::vec3(70.0f, -2.0f, 0.0f);
    landing.reset(approach);

    bool touchedDown = false;
    float touchdownSpeed = 0.0f;
    flight::FixedWingControls approachControls;
    approachControls.throttle = 0.0f;
    for (int i = 0; i < 30 * 120 && !touchedDown; ++i)
    {
        landing.setControls(approachControls);
        landing.advance(1.0f / 120.0f);
        if (landing.consumeTouchdownEvent())
        {
            touchedDown = true;
            touchdownSpeed = landing.lastTouchdownVerticalSpeed();
        }
    }
    if (!touchedDown || !landing.state().grounded)
    {
        std::cerr << "[FixedWingFlightTest] aircraft did not touch down\n";
        return 1;
    }

    const float speedAtTouchdown = glm::length(landing.state().velocity);
    landing.setBrakes(true);
    for (int i = 0; i < 8 * 120; ++i)
    {
        landing.setControls(approachControls);
        landing.advance(1.0f / 120.0f);
    }
    const float speedAfterBraking = glm::length(landing.state().velocity);
    std::cout << "[FixedWingFlightTest] touchdown vertical speed=" << touchdownSpeed
              << " m/s; braking speed " << speedAtTouchdown << " -> " << speedAfterBraking << " m/s\n";
    if (speedAfterBraking > 10.0f || speedAfterBraking > speedAtTouchdown)
    {
        std::cerr << "[FixedWingFlightTest] brakes did not decelerate the aircraft\n";
        return 1;
    }

    // 直接翼面：固定小偏角应产生角速度，回中后阻尼使角速度收敛。
    flight::FixedWingDynamics direct;
    direct.reset(levelState);
    direct.setDirectSurfaceMode(true);
    flight::ControlSurfaceAngles directTarget;
    directTarget.stabilator = -0.04f;
    direct.setDirectSurfaceTarget(directTarget);
    flight::FixedWingControls directControls;
    directControls.throttle = 0.5f;
    for (int i = 0; i < 120; ++i)
    {
        direct.setControls(directControls);
        direct.advance(1.0f / 120.0f);
    }
    const float directPitchRate = direct.state().angularVelocity.z;
    const float directPitch = pitchOf(direct.state());
    direct.setDirectSurfaceTarget({});
    for (int i = 0; i < 120; ++i)
    {
        direct.setControls(directControls);
        direct.advance(1.0f / 120.0f);
    }
    const float directRateAfterNeutral = direct.state().angularVelocity.z;
    std::cout << "[FixedWingFlightTest] direct stabilator: pitch=" << directPitch * 180.0f / glm::pi<float>()
              << " deg rate=" << directPitchRate << " -> " << directRateAfterNeutral << " rad/s\n";
    if (!std::isfinite(directPitchRate) || !std::isfinite(directRateAfterNeutral) ||
        std::fabs(directPitchRate) < 1.0e-3f || std::fabs(directRateAfterNeutral) > std::fabs(directPitchRate))
    {
        std::cerr << "[FixedWingFlightTest] direct surface did not produce damped motion\n";
        return 1;
    }

    // 起落架状态：离地 1.5 m 且未收起时应比收起减速更快。
    flight::RigidBodyState low = levelState;
    low.position.y = direct.parameters().gearHeight + 1.5f;
    low.velocity = glm::vec3(70.0f, 0.0f, 0.0f);
    flight::FixedWingDynamics gearDownDynamics;
    flight::FixedWingDynamics gearUpDynamics;
    gearDownDynamics.reset(low);
    gearUpDynamics.reset(low);
    gearDownDynamics.setGearDown(true);
    gearUpDynamics.setGearDown(false);
    flight::FixedWingControls lowControls;
    lowControls.throttle = 0.5f;
    for (int i = 0; i < 120; ++i)
    {
        gearDownDynamics.setControls(lowControls);
        gearUpDynamics.setControls(lowControls);
        gearDownDynamics.advance(1.0f / 120.0f);
        gearUpDynamics.advance(1.0f / 120.0f);
    }
    const float gearDownSpeed = glm::length(gearDownDynamics.state().velocity);
    const float gearUpSpeed = glm::length(gearUpDynamics.state().velocity);
    std::cout << "[FixedWingFlightTest] gear low-altitude speed: down=" << gearDownSpeed << " up=" << gearUpSpeed
              << " m/s\n";
    if (gearDownSpeed >= gearUpSpeed)
    {
        std::cerr << "[FixedWingFlightTest] gear-down low-altitude drag did not decelerate\n";
        return 1;
    }

    // 离地 3.2 m 内自动改平俯仰，flaperon 归零。
    flight::FixedWingDynamics autoLevel;
    low.position.y = autoLevel.parameters().gearHeight + 1.5f;
    low.orientation = glm::angleAxis(glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    low.velocity = glm::vec3(70.0f, -2.0f, 0.0f);
    autoLevel.reset(low);
    flight::FixedWingControls levelControls;
    levelControls.throttle = 0.5f;
    for (int i = 0; i < 120; ++i)
    {
        autoLevel.setControls(levelControls);
        autoLevel.advance(1.0f / 120.0f);
    }
    const float leveledPitch = pitchOf(autoLevel.state());
    const float leveledFlaperon = autoLevel.controlSurfaces().flaperonRight;
    std::cout << "[FixedWingFlightTest] low-altitude level: pitch=" << leveledPitch * 180.0f / glm::pi<float>()
              << " deg flaperon=" << leveledFlaperon << " rad\n";
    if (std::fabs(leveledPitch) > 3.0f * glm::pi<float>() / 180.0f || std::fabs(leveledFlaperon) > 0.01f)
    {
        std::cerr << "[FixedWingFlightTest] low-altitude level/flaperon failed\n";
        return 1;
    }

    // 地面低速：不允许抬头，flaperon 必须水平；70 m/s 以下锁俯仰。
    flight::FixedWingDynamics groundRoll;
    flight::RigidBodyState groundState;
    groundState.position = glm::vec3(0.0f, groundRoll.parameters().gearHeight, 0.0f);
    groundState.velocity = glm::vec3(30.0f, 0.0f, 0.0f);
    groundState.grounded = true;
    groundRoll.reset(groundState);
    flight::FixedWingControls groundControls;
    groundControls.pitch = 1.0f;
    groundControls.roll = 1.0f;
    groundControls.throttle = 0.8f;
    for (int i = 0; i < 120; ++i)
    {
        groundRoll.setControls(groundControls);
        groundRoll.advance(1.0f / 120.0f);
    }
    const float groundPitch = pitchOf(groundRoll.state());
    const glm::vec3 groundRight = groundRoll.state().orientation * glm::vec3(0.0f, 0.0f, 1.0f);
    const float groundBank = std::asin(std::clamp(groundRight.y, -1.0f, 1.0f));
    const float groundFlaperon = groundRoll.controlSurfaces().flaperonRight;
    const float groundVerticalSpeed = groundRoll.state().velocity.y;
    std::cout << "[FixedWingFlightTest] ground low-speed: pitch=" << groundPitch * 180.0f / glm::pi<float>()
              << " deg bank=" << groundBank * 180.0f / glm::pi<float>() << " deg flaperon=" << groundFlaperon
              << " vy=" << groundVerticalSpeed << " m/s\n";
    if (std::fabs(groundPitch) > 0.5f * glm::pi<float>() / 180.0f ||
        std::fabs(groundBank) > 0.5f * glm::pi<float>() / 180.0f || std::fabs(groundFlaperon) > 0.01f ||
        std::fabs(groundVerticalSpeed) > 0.01f)
    {
        std::cerr << "[FixedWingFlightTest] ground pitch/roll/vertical/flaperon inhibit failed\n";
        return 1;
    }
    return 0;
}

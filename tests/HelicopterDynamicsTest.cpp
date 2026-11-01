#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "HelicopterDynamics.h"
#include "ThirdPersonCamera.h"
#include "Camera.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
flight::RigidBodyState airborne(float height = 40)
{
    flight::RigidBodyState s;
    s.position.y = height;
    return s;
}
void run(flight::HelicopterDynamics& d, int frames, float dt)
{
    for (int i = 0; i < frames; ++i)
        d.advance(dt);
}
void physics()
{
    using namespace flight;
    FlightControls controls;
    HelicopterDynamics hover;
    hover.reset(airborne());
    hover.setControls(controls);
    run(hover, 600, 1.0f / 60);
    require(glm::length(hover.state().position - glm::vec3(0, 40, 0)) < .001f, "Neutral controls do not hover");
    auto angledState = airborne(4);
    angledState.orientation = glm::angleAxis(glm::radians(-24.0f), glm::vec3(0, 1, 0));
    HelicopterDynamics angledHover;
    angledHover.reset(angledState);
    controls.heading = glm::radians(-24.0f);
    angledHover.setControls(controls);
    run(angledHover, 1800, 1.0f / 60);
    require(glm::length(angledHover.state().position - angledState.position) < .005f,
            "Initial yaw produces unwanted hover drift");

    HelicopterParameters noDrag;
    noDrag.linearDrag = 0;
    noDrag.quadraticDrag = 0;
    HelicopterDynamics falling(noDrag);
    falling.reset(airborne(100), 0);
    controls.engineOn = false;
    falling.setControls(controls);
    run(falling, 120, 1.0f / 120);
    require(std::abs(falling.state().position.y - (100 - 4.905f)) < .05f, "Gravity is not integrated as acceleration");

    controls = {};
    controls.horizontalVelocity = {10, 0, 0};
    HelicopterDynamics forward;
    forward.reset(airborne());
    forward.setControls(controls);
    run(forward, 300, 1.0f / 60);
    require(forward.state().position.x > 15 && forward.state().velocity.x > 4,
            "Cyclic tilt does not produce forward flight");
    require(std::abs(forward.state().position.y - 40) < 2, "Assisted forward flight loses too much altitude");
    controls.horizontalVelocity = {};
    forward.setControls(controls);
    run(forward, 360, 1.0f / 60);
    require(glm::length(forward.state().velocity) < .3f, "Neutral assisted controls do not brake to hover");

    HelicopterDynamics sideways;
    sideways.reset(airborne());
    controls.horizontalVelocity = {0, 0, -10};
    sideways.setControls(controls);
    run(sideways, 300, 1.0f / 60);
    require(sideways.state().position.z < -15 && std::abs(sideways.state().position.x) < .1f,
            "Strafing uses the wrong axis");

    HelicopterDynamics climb;
    climb.reset(airborne());
    controls = {};
    controls.climb = 1;
    climb.setControls(controls);
    run(climb, 240, 1.0f / 60);
    require(climb.state().position.y > 50, "Collective command does not climb");
    controls.climb = 0;
    climb.setControls(controls);
    run(climb, 240, 1.0f / 60);
    require(std::abs(climb.state().velocity.y) < .1f, "Climb release does not hold height");

    HelicopterDynamics yaw;
    yaw.reset(airborne());
    controls = {};
    controls.heading = glm::radians(90.0f);
    yaw.setControls(controls);
    run(yaw, 240, 1.0f / 60);
    const auto heading = yaw.state().orientation * glm::vec3(1, 0, 0);
    require(heading.z < -.98f && std::abs(heading.x) < .1f, "Yaw torque does not track heading");

    HelicopterDynamics fps30, fps60, fps144;
    controls = {};
    controls.horizontalVelocity = {8, 0, 3};
    controls.climb = .4f;
    controls.heading = .5f;
    for (auto* d : {&fps30, &fps60, &fps144})
    {
        d->reset(airborne());
        d->setControls(controls);
    }
    run(fps30, 180, 1.0f / 30);
    run(fps60, 360, 1.0f / 60);
    run(fps144, 864, 1.0f / 144);
    require(glm::length(fps30.state().position - fps60.state().position) < .002f &&
                glm::length(fps144.state().position - fps60.state().position) < .002f,
            "Physics changes with rendering FPS");
    require(std::abs(glm::length(fps60.state().orientation) - 1) < .0001f, "Orientation quaternion lost normalization");

    HelicopterDynamics land;
    land.reset(airborne(8), 0);
    controls = {};
    controls.engineOn = false;
    land.setControls(controls);
    run(land, 600, 1.0f / 60);
    require(land.state().grounded && std::abs(land.state().velocity.y) < .001f,
            "Aircraft does not settle on the ground");
    for (auto point : land.parameters().landingPoints)
        require((land.state().position + land.state().orientation * point).y >= -.0001f,
                "Landing gear penetrates ground");
    controls = {};
    controls.climb = 1;
    land.setControls(controls);
    run(land, 240, 1.0f / 60);
    require(!land.state().grounded && land.state().position.y > 5, "Restarted engine cannot lift off from ground");

    controls = {};
    controls.engineOn = false;
    hover.setControls(controls);
    // [飞行物理验证] 停机等待至少五个响应时间常数，覆盖约 99.3% 的转速衰减；
    // 调慢 spoolResponse 后仍验证失去升力，避免把旧参数下的两秒等待当成固定性能要求。
    require(hover.parameters().spoolResponse > 0, "Engine spool response must be positive");
    const float shutdownSeconds = std::max(2.0f, 5.0f / hover.parameters().spoolResponse);
    run(hover, static_cast<int>(std::ceil(shutdownSeconds * 60)), 1.0f / 60);
    require(hover.rotorPower() < .01f && hover.state().position.y < 35, "Engine shutdown does not remove lift");
    const auto before = hover.state().position;
    hover.advance(std::numeric_limits<float>::quiet_NaN());
    require(glm::length(before - hover.state().position) == 0, "Invalid frame time poisons physics");
}
void chaseCamera()
{
    Camera camera;
    ThirdPersonCamera chase;
    const glm::vec3 target(0, 2, 0);
    chase.enter(target, {1, 0, 0}, camera);
    require(camera.position().x < target.x, "Chase camera is not behind the vehicle");
    require(glm::dot(camera.front(), glm::normalize(target - camera.position())) > .999f,
            "Chase camera misses its target");
    require(glm::length(chase.movementDirection(1, 1)) <= 1.0001f, "Diagonal movement is faster");
    chase.mouseMovement(900, 0);
    require(chase.movementDirection(1, 0).z > .999f, "Movement does not follow orbit yaw");
    chase.mouseMovement(0, 100000);
    chase.zoom(10000);
    chase.update({4, 2, 0}, 1.0f / 60, camera);
    require(chase.distance() >= 3.5f && std::isfinite(camera.position().y),
            "Camera pitch / near distance clamp failed");
    chase.zoom(-10000);
    require(chase.distance() <= 12, "Camera zoom out is unbounded");
    chase.mouseMovement(0, -100000);
    chase.update({0, .2f, 0}, 1, camera);
    require(camera.position().y >= .349f, "Orbit camera penetrates the ground");
    chase.enter({0, 10, 0}, {1, 0, 0}, camera);
    chase.mouseMovement(0, -900);
    chase.update({0, 10, 0}, 1, camera);
    require(camera.position().y < 9, "Camera cannot orbit below an airborne vehicle");
    require(std::isfinite(camera.front().y), "Positive orbit pitch makes the view invalid");
    chase.mouseMovement(0, 100000);
    chase.update({0, 10, 0}, 1, camera);
    require(camera.position().y > 11, "Camera cannot orbit above the vehicle");
}
}
int main()
{
    try
    {
        physics();
        chaseCamera();
        std::cout
            << "Flight: hover, gravity, cyclic, climb, yaw, braking, landing, 30/60/144 FPS and chase camera passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

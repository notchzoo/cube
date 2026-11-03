#include "Application.h"
#include "AttackHelicopterGeometry.h"
#include "AttackHelicopterObject.h"
#include "FixedWingObject.h"
#include "GltfLoader.h"
#include "GltfObject.h"
#include "Su57Rig.h"
#include "GlUtil.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
// --- 场景与相机常量 -------------------------------------------------------
constexpr int kShadowMapSize = 2048;
// 离屏截图同样走 MSAA：与窗口里的 4x 抗锯齿一致，基准图才等于“窗内所见”。
constexpr int kScreenshotSamples = 4;
constexpr float kCameraFieldOfViewDegrees = 50.0f;
constexpr float kCameraNearPlane = 0.1f;
constexpr float kCameraFarPlane = 4000.0f;
constexpr float kGroundHalfExtent = 1200.0f;
constexpr float kCubeScale = 0.8f;               // 单位立方体 -> 边长 1.6
constexpr float kCubeCenterHeight = 1.95f;       // 中心高度；底面 = 该值 - kCubeScale
constexpr float kCubeInitialSpinDegrees = 28.0f; // 初始自转角：三个面同时可见
const glm::vec3 kCubeSpinAxis(1.0f, 1.0f, 1.0f); // 旋转轴（工作区当前值；改回 (0,1,0) 即绕竖轴）

// --- 交互常量 -------------------------------------------------------------
constexpr float kMaxFrameDelta = 0.1f;
constexpr float kFastMoveMultiplier = 2.5f;
constexpr float kWheelElevationStep = 0.25f;

void printGlfwError(int code, const char* description)
{
    std::cerr << "[GLFW] error " << code << ": " << (description ? description : "") << '\n';
}

/// 写 24 位 BMP。输入是 glReadPixels 的 RGBA 数据，行序自下而上，正好是 BMP 的行序。
bool writeBmp24(const std::string& path, int width, int height, const std::vector<unsigned char>& rgbaPixels)
{
    if (width <= 0 || height <= 0 || rgbaPixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4u)
    {
        return false;
    }

    const int rowBytes = (width * 3 + 3) & ~3;
    const int imageSize = rowBytes * height;
    const int fileSize = 54 + imageSize;

    std::vector<unsigned char> header(54, 0);
    header[0] = 'B';
    header[1] = 'M';
    const auto putU32 = [&header](size_t offset, unsigned int value)
    {
        header[offset + 0] = static_cast<unsigned char>(value & 0xFFu);
        header[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xFFu);
        header[offset + 2] = static_cast<unsigned char>((value >> 16) & 0xFFu);
        header[offset + 3] = static_cast<unsigned char>((value >> 24) & 0xFFu);
    };
    putU32(2, static_cast<unsigned int>(fileSize));
    putU32(10, 54);
    putU32(14, 40);
    putU32(18, static_cast<unsigned int>(width));
    putU32(22, static_cast<unsigned int>(height));
    putU32(26, 0x00180001u); // planes = 1, bits per pixel = 24
    putU32(34, static_cast<unsigned int>(imageSize));
    putU32(38, 2835);
    putU32(42, 2835);

    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open())
    {
        return false;
    }
    stream.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));

    std::vector<unsigned char> row(static_cast<size_t>(rowBytes), 0);
    for (int y = 0; y < height; ++y)
    {
        const unsigned char* source = rgbaPixels.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
        for (int x = 0; x < width; ++x)
        {
            row[static_cast<size_t>(x) * 3u + 0u] = source[static_cast<size_t>(x) * 4u + 2u];
            row[static_cast<size_t>(x) * 3u + 1u] = source[static_cast<size_t>(x) * 4u + 1u];
            row[static_cast<size_t>(x) * 3u + 2u] = source[static_cast<size_t>(x) * 4u + 0u];
        }
        stream.write(reinterpret_cast<const char*>(row.data()), rowBytes);
    }

    return stream.good();
}
}

// --- 生命周期 -------------------------------------------------------------

Application::Application(int width, int height, const char* title)
    : m_title(title != nullptr ? title : "RenderingCube"),
      m_windowWidth(width > 0 ? width : 1400),
      m_windowHeight(height > 0 ? height : 900)
{
    m_camera.setPosition(glm::vec3(0.0f, 3.0f, 6.6f));
    m_camera.setYawPitch(-90.0f, -20.0f);

    // 场景对象（骰子、地面）在 initialize() 里创建：它们需要 GL 上下文。
    // 皮肤、点数、自转默认值都在 DiceObject 内部，这里只给"摆在哪里、多大、初始角度"。
}

Application::~Application()
{
    releaseResources();
}

bool Application::initialize()
{
    if (!createWindow())
    {
        return false;
    }
    if (!loadShaders())
    {
        releaseResources();
        return false;
    }

    createScene();

    m_lastFrameTime = glfwGetTime();
    m_initialized = true;
    return true;
}

void Application::run(float durationSeconds)
{
    if (!m_initialized)
    {
        return;
    }

    const double started = glfwGetTime();
    while (glfwWindowShouldClose(m_window) == GLFW_FALSE)
    {
        const double now = glfwGetTime();
        float deltaSeconds = static_cast<float>(now - m_lastFrameTime);
        m_lastFrameTime = now;
        // 断点/拖动窗口会让 delta 变得很大，夹紧避免相机瞬移。
        deltaSeconds = std::clamp(deltaSeconds, 0.0f, kMaxFrameDelta);

        // 着色器热重载：内部按 0.4 秒节流检查 mtime，命中才重新编译链接。
        m_shaderWatcher.poll(now);

        // 覆盖层显示的帧时间做指数平滑，避免数字每帧乱跳。
        m_smoothedFrameMilliseconds += (deltaSeconds * 1000.0f - m_smoothedFrameMilliseconds) * 0.1f;

        // [载具更新] 顺序固定：采集驾驶输入 -> 飞机物理/动画 -> 跟随相机 -> 渲染。
        // 不要在 renderScene 的两个 pass 中推进物理或动画，否则一帧会积分两次。
        processInput(deltaSeconds);
        // 滚轮只累加目标高度，这里每帧平滑逼近，效果和按键一样连续。
        if (!m_driving && !m_jetFlying)
            m_camera.updateElevation(deltaSeconds);

        // 自转由对象自己推进：暂停时角度保留，恢复从原处继续。
        if (m_dice != nullptr)
        {
            m_dice->update(deltaSeconds);
        }
        if (m_helicopter != nullptr)
            m_helicopter->update(deltaSeconds);
        if (m_jet != nullptr && m_jetFlying)
        {
            m_jet->update(deltaSeconds);
            if (m_jet->consumeTouchdownEvent())
            {
                m_jetHasTouchedDown = true;
                m_jetTouchdownSpeed = m_jet->dynamics().lastTouchdownVerticalSpeed();
                m_jetHardLanding = m_jetTouchdownSpeed < -8.0f;

                // 接地后自动收油门、断开自驾并刹车，避免自动油门立刻把飞机重新拉起来。
                m_jetAutopilot = false;
                m_jet->setAutopilot(false);
                m_jetAutoThrottle = false;
                m_jetThrottle = 0.0f;
                m_jet->setThrottleControl(true, 0.0f);
                m_jetBrakes = true;
                m_jet->setBrakes(true);
                std::cout << "[Jet] touchdown " << m_jetTouchdownSpeed << " m/s"
                          << (m_jetHardLanding ? " (hard landing)" : "") << "; brakes on, B release\n";
            }
        }
        updateVehicleCamera(deltaSeconds);

        renderScene();

        glfwSwapBuffers(m_window);
        glfwPollEvents();
        if (durationSeconds > 0 && glfwGetTime() - started >= durationSeconds)
            break;
    }
}

void Application::setHelicopterRotorsEnabled(bool enabled)
{
    if (m_helicopter)
        m_helicopter->setRotorsEnabled(enabled);
}

void Application::setHelicopterPose(float mainRotor, float tailRotor, float tailPitch, float stabilator)
{
    if (!m_helicopter)
        return;
    m_helicopter->setRotorAngles(mainRotor, tailRotor);
    m_helicopter->setControlAngles(tailPitch, stabilator);
}

// [驾驶交互] G 与 --drive 共用的唯一切换入口；距离检查必须先于任何驾驶状态修改。
// 进入：检查座舱距离 -> 启用物理 -> 记录机头航向 -> 初始化第三人称相机。
// 退出：清空驾驶输入，保留飞机物理状态，让自由相机留在当前观察位置。
void Application::setDriving(bool enabled)
{
    if (!m_helicopter || m_driving == enabled)
        return;
    if (enabled && !m_helicopter->canEnterFrom(m_camera.position()))
    {
        updateInteractionPrompt();
        std::cout << "[Driving] out of reach; approach the cockpit before pressing G\n";
        return;
    }
    m_driving = enabled;
    m_helicopter->clearFlightControls();
    m_firstMouseEvent = true;
    m_titleUpdateTime = 0;
    if (enabled)
    {
        m_helicopter->enableFlightPhysics();
        const auto aircraftForward = m_helicopter->forwardDirection();
        m_pilotHeading = std::atan2(-aircraftForward.z, aircraftForward.x);
        m_chaseCamera.setFollowGain(7.0f);
        m_chaseCamera.setHeadingFollow(false);
        m_chaseCamera.enter(m_helicopter->focusPoint(), aircraftForward, m_camera);
        std::cout << "[Driving] entered; WASD move, Space/Ctrl climb, Q/E yaw, G exit\n";
    }
    else
    {
        // Leave the free camera at its current safe chase-camera position.
        m_camera.setPosition(m_camera.position());
        updateInteractionPrompt();
        std::cout << "[Driving] exited; free camera restored\n";
    }
}

// [喷气机] J 键与 --fly-jet 共用入口。进入时启用固定翼物理并切到第三人称跟随；
// 退出只暂停更新，保留飞机当前状态，再次进入从原处继续。
void Application::setJetFlying(bool enabled)
{
    if (m_jet == nullptr || m_jetFlying == enabled)
        return;

    if (enabled && m_driving)
        setDriving(false);

    m_jetFlying = enabled;
    m_firstMouseEvent = true;
    m_titleUpdateTime = 0.0f;

    if (enabled)
    {
        m_jet->enableFlight();
        m_jetAutopilot = true;
        m_jet->setAutopilot(true);
        m_jetAutoThrottle = true;
        m_jetThrottle = 0.0f;
        m_jet->setThrottleControl(false, 0.0f);
        m_jetBrakes = false;
        m_jet->setBrakes(false);
        m_jetHasTouchedDown = false;
        m_jetHardLanding = false;
        m_jetTouchdownSpeed = 0.0f;
        m_chaseCamera.setHeadingFollow(true);
        m_chaseCamera.enter(m_jet->focusPoint(), m_jet->forwardDirection(), m_camera);
        m_chaseCamera.setFollowGain(20.0f); // 高速目标：减小跟随滞后，同时避免过度放大帧间抖动
        m_chaseCamera.zoom(-2.0f);
        m_chaseCamera.mouseMovement(0.0f, -100.0f); // 从默认 -18° 抬到约 -8°，避免俯视地面
        std::cout << "[Jet] flight started; W/S pitch, A/D roll, Q/E yaw, P autopilot, "
                     "Shift/Ctrl throttle, T auto-throttle, B brakes, Backspace reset, J exit\n";
    }
    else
    {
        std::cout << "[Jet] flight paused\n";
    }
}

// [重置] 回到初始停机状态；油门/自驾/刹车都归零，按 B/P/T 再重新起飞。
void Application::resetJet()
{
    if (m_jet == nullptr)
        return;

    if (m_surfaceTest)
        setSurfaceTest(false);

    m_jet->resetFlight();
    m_jetAutopilot = false;
    m_jet->setAutopilot(false);
    m_jetAutoThrottle = false;
    m_jetThrottle = 0.0f;
    m_jet->setThrottleControl(true, 0.0f);
    m_jetBrakes = true;
    m_jet->setBrakes(true);
    m_jetHasTouchedDown = false;
    m_jetHardLanding = false;
    m_jetTouchdownSpeed = 0.0f;

    if (m_jetFlying)
    {
        m_chaseCamera.setHeadingFollow(true);
        m_chaseCamera.enter(m_jet->focusPoint(), m_jet->forwardDirection(), m_camera);
    }
    std::cout << "[Jet] reset; B release brakes, P autopilot, T auto-throttle\n";
}

// [翼面检查] 静止状态下按 K 进入：1-5 选择翼面组，[ ] 调整，0 归零。// 这里只驱动视觉偏角，不参与气动力矩；飞行模式会覆盖它，因此要求先退出飞行。
void Application::setSurfaceTest(bool enabled)
{
    if (m_jet == nullptr || m_surfaceTest == enabled)
        return;
    if (enabled && m_jetFlying)
    {
        std::cout << "[Surface] exit jet flight first (J), then press K\n";
        return;
    }

    m_surfaceTest = enabled;
    if (enabled)
    {
        m_surfaceTestAngles.fill(0.0f);
        m_surfaceGroup = 0;
        m_jet->clearSurfaceTestAngles();

        // 后右上方视角：能同时看到副翼、平尾、垂尾和喷口。
        const glm::vec3 focus = m_jet->focusPoint();
        m_camera.setPosition(focus + glm::vec3(4.0f, 2.5f, 4.0f));
        const glm::vec3 direction = glm::normalize(focus - m_camera.position());
        m_camera.setYawPitch(glm::degrees(std::atan2(direction.z, direction.x)),
                             glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f))));
        std::cout << "[Surface] inspection on; 1-5 select, left/right arrows or [ ] adjust, 0 reset, K exit\n";
    }
    else
    {
        m_jet->clearSurfaceTestAngles();
        std::cout << "[Surface] inspection off\n";
    }
}

void Application::updateSurfaceTest(float deltaSeconds)
{
    if (!m_surfaceTest || m_jet == nullptr)
        return;

    for (int i = 0; i < 5; ++i)
    {
        const bool down = glfwGetKey(m_window, GLFW_KEY_1 + i) == GLFW_PRESS;
        if (down && !m_surfaceDigitWasDown[i])
            m_surfaceGroup = i;
        m_surfaceDigitWasDown[i] = down;
    }

    const bool resetDown = glfwGetKey(m_window, GLFW_KEY_0) == GLFW_PRESS;
    if (resetDown && !m_surfaceResetWasDown)
        m_surfaceTestAngles[m_surfaceGroup] = 0.0f;
    m_surfaceResetWasDown = resetDown;

    const bool increase = glfwGetKey(m_window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS ||
                          glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    const bool decrease = glfwGetKey(m_window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS ||
                          glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS;

    float delta = 0.0f;
    if (increase)
        delta += 25.0f * deltaSeconds + (m_surfaceAdjustWasDown[0] ? 0.0f : 2.0f);
    if (decrease)
        delta -= 25.0f * deltaSeconds + (m_surfaceAdjustWasDown[1] ? 0.0f : 2.0f);
    m_surfaceAdjustWasDown[0] = increase;
    m_surfaceAdjustWasDown[1] = decrease;

    static constexpr float kMinDegrees[5] = {-25.0f, 0.0f, -25.0f, -25.0f, -14.0f};
    static constexpr float kMaxDegrees[5] = {25.0f, 20.0f, 25.0f, 25.0f, 14.0f};
    if (delta != 0.0f)
    {
        float& angle = m_surfaceTestAngles[m_surfaceGroup];
        angle = std::clamp(angle + delta, kMinDegrees[m_surfaceGroup], kMaxDegrees[m_surfaceGroup]);
    }

    for (int group = 0; group < 5; ++group)
        m_jet->setSurfaceTestAngle(group, glm::radians(m_surfaceTestAngles[group]));
}

// [驾驶交互] 自由观察时的标题提示；锚点和距离读取当前飞机 Model，因此会跟随飞机移动。
void Application::updateInteractionPrompt()
{
    if (!m_window || !m_helicopter || m_driving)
        return;
    std::string prompt = m_title;
    if (m_helicopter->canEnterFrom(m_camera.position()))
        prompt += " | 按 G 驾驶直升机";
    else
    {
        const float unitsPerMeter = glm::length(glm::vec3(m_helicopter->modelMatrix()[0])) / 2;
        const float metres =
            glm::distance(m_camera.position(), m_helicopter->interactionPoint()) / std::max(.0001f, unitsPerMeter);
        std::ostringstream distance;
        distance << std::fixed << std::setprecision(1) << metres;
        prompt += " | 靠近座舱后按 G 驾驶 | 距离 " + distance.str() + " m";
    }
    glfwSetWindowTitle(m_window, prompt.c_str());
}

// [第三人称相机] run 在飞机 update 之后调用；此处跟随最新机体位置，并调整阴影覆盖范围。
// 自由模式只更新交互提示，不把自由相机拉回飞机。
void Application::updateVehicleCamera(float dt)
{
    if (m_jetFlying && m_jet)
    {
        const glm::vec3 focus = m_jet->focusPoint();
        const glm::vec3 direction = m_lightRig.normalizedDirection();
        const glm::vec3 shadow = focus + direction * (focus.y / std::max(.15f, -direction.y));
        m_lightRig.setShadowFocus((focus + shadow) * .5f, std::max(9.0f, glm::distance(focus, shadow) * .6f + 5));

        const glm::vec3 forward = m_jet->forwardDirection();
        m_chaseCamera.followHeading(glm::degrees(std::atan2(forward.z, forward.x)), dt);
        m_chaseCamera.update(focus, dt, m_camera);

        m_titleUpdateTime -= dt;
        if (m_titleUpdateTime <= 0)
        {
            char loadFactorText[16] = {};
            std::snprintf(loadFactorText, sizeof(loadFactorText), "%.1f", m_jet->dynamics().loadFactor());
            const std::string title =
                "喷气机 | W/S俯仰 A/D滚转 Q/E偏航 P自驾 Shift/Ctrl油门 T自动油门 J退出 | " +
                std::to_string(static_cast<int>(m_jet->speedKmh())) + " km/h | 高度 " +
                std::to_string(static_cast<int>(m_jet->altitudeMeters())) + " m | 过载 " + loadFactorText + "g | 油门 " +
                std::to_string(static_cast<int>(m_jet->dynamics().throttle() * 100.0f)) + "% " +
                (m_jetAutoThrottle ? "自动" : "手动") + (m_jetAutopilot ? " | 自驾开" : " | 自驾关");
            glfwSetWindowTitle(m_window, title.c_str());
            m_titleUpdateTime = .25f;
        }
        return;
    }

    if (!m_helicopter)
        return;
    if (m_helicopter->flightPhysicsEnabled())
    {
        const auto position = glm::vec3(m_helicopter->modelMatrix()[3]);
        const auto direction = m_lightRig.normalizedDirection();
        const auto shadow = position + direction * (position.y / std::max(.15f, -direction.y));
        m_lightRig.setShadowFocus((position + shadow) * .5f, std::max(9.0f, glm::distance(position, shadow) * .6f + 5));
    }
    m_titleUpdateTime -= dt;
    if (!m_driving)
    {
        if (m_titleUpdateTime <= 0)
        {
            updateInteractionPrompt();
            m_titleUpdateTime = .25f;
        }
        return;
    }
    m_chaseCamera.update(m_helicopter->focusPoint(), dt, m_camera);
    if (m_titleUpdateTime <= 0)
    {
        const auto& state = m_helicopter->dynamics().state();
        const std::string title = "驾驶 | WASD移动 空格升/Ctrl降 Q/E转向 G退出 | " +
                                  std::to_string(static_cast<int>(glm::length(state.velocity) * 3.6f)) +
                                  " km/h | 高度 " + std::to_string(static_cast<int>(state.position.y)) + " m";
        glfwSetWindowTitle(m_window, title.c_str());
        m_titleUpdateTime = .25f;
    }
}

// --- 配置接口 -------------------------------------------------------------

void Application::setDirectionalLight(const DirectionalLight& light)
{
    m_lightRig.setLight(light);
}

void Application::setLightDirection(const glm::vec3& direction)
{
    m_lightRig.setDirection(direction);
}

void Application::setCubeMaterial(const PBRMaterial& material)
{
    if (m_dice == nullptr)
    {
        std::cerr << "[Application] setCubeMaterial ignored before initialize()\n";
        return;
    }
    m_dice->setAllFaceMaterials(material);
}

void Application::setCubeFaceMaterial(int face, const PBRMaterial& material)
{
    if (m_dice == nullptr)
    {
        std::cerr << "[Application] setCubeFaceMaterial ignored before initialize()\n";
        return;
    }
    m_dice->setFaceMaterial(face, material);
}

PBRMaterial Application::cubeFaceMaterial(int face) const
{
    return (m_dice != nullptr) ? m_dice->faceMaterial(face) : PBRMaterial{};
}

void Application::setGroundMaterial(const PBRMaterial& material)
{
    if (m_ground != nullptr)
    {
        m_ground->setMaterial(material);
    }
}

void Application::setPipsEnabled(bool enabled)
{
    if (m_dice != nullptr)
    {
        m_dice->setPipsEnabled(enabled);
    }
}

// --- 太阳 -----------------------------------------------------------------

void Application::setSunStyle(const SunStyle& style)
{
    m_sun.setStyle(style);
}

const SunStyle& Application::sunStyle() const
{
    return m_sun.style();
}

bool Application::setSunTexture(const std::string& path)
{
    return m_sun.setTextureFromBmp(path);
}

void Application::clearSunTexture()
{
    m_sun.clearTexture();
}

void Application::setCameraPose(const glm::vec3& position, float yawDegrees, float pitchDegrees)
{
    m_camera.setPosition(position);
    m_camera.setYawPitch(yawDegrees, pitchDegrees);
    updateInteractionPrompt();
}

void Application::setReflectionStrength(float strength)
{
    m_reflectionStrength = std::clamp(strength, 0.0f, 1.0f);
}

void Application::setObjectOpacity(float opacity)
{
    if (m_dice != nullptr)
    {
        m_dice->setOpacity(opacity);
    }
}

float Application::objectOpacity() const
{
    return (m_dice != nullptr) ? m_dice->opacity() : 1.0f;
}

bool Application::pipsEnabled() const
{
    return (m_dice != nullptr) && m_dice->pipsEnabled();
}

void Application::enableShadows(bool enabled)
{
    m_shadowsEnabled = enabled;
}

// --- 自转 -----------------------------------------------------------------

void Application::setCubeSpinEnabled(bool enabled)
{
    if (m_dice != nullptr)
    {
        m_dice->setSpinEnabled(enabled);
    }
}

void Application::toggleCubeSpin()
{
    if (m_dice != nullptr)
    {
        m_dice->toggleSpin();
    }
}

void Application::setCubeSpinSpeed(float radiansPerSecond)
{
    if (m_dice != nullptr)
    {
        m_dice->setSpinSpeed(radiansPerSecond);
    }
}

void Application::setCubeSpinAngle(float radians)
{
    if (m_dice != nullptr)
    {
        m_dice->setSpinAngle(radians);
    }
}

bool Application::cubeSpinEnabled() const
{
    return (m_dice != nullptr) && m_dice->spinEnabled();
}

float Application::cubeSpinAngle() const
{
    return (m_dice != nullptr) ? m_dice->spinAngle() : 0.0f;
}

float Application::cubeSpinSpeed() const
{
    return (m_dice != nullptr) ? m_dice->spinSpeed() : 0.0f;
}

// --- 窗口与资源 -----------------------------------------------------------

void Application::setMouseLookEnabled(bool enabled)
{
    m_mouseLookEnabled = enabled;
    m_firstMouseEvent = true;

    if (m_window == nullptr)
    {
        return;
    }

    glfwSetInputMode(m_window, GLFW_CURSOR, enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    const int rawMotion = (enabled && glfwRawMouseMotionSupported() == GLFW_TRUE) ? GLFW_TRUE : GLFW_FALSE;
    glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, rawMotion);
}

void Application::setAssetDirectory(const std::string& directory)
{
    m_assetDirectory = directory;
}

void Application::setWindowVisible(bool visible)
{
    m_windowVisible = visible;
}

bool Application::createWindow()
{
    glfwSetErrorCallback(printGlfwError);
    if (glfwInit() == GLFW_FALSE)
    {
        std::cerr << "[Application] glfwInit failed\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, m_windowVisible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, m_title.c_str(), nullptr, nullptr);
    if (m_window == nullptr)
    {
        std::cerr << "[Application] window creation failed (OpenGL 3.3 Core required)\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
    {
        std::cerr << "[Application] gladLoadGLLoader failed\n";
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
        return false;
    }

    glfwSwapInterval(1);

    // HiDPI：窗口尺寸与 framebuffer 尺寸不一定相等，viewport 必须用后者。
    glfwGetFramebufferSize(m_window, &m_framebufferWidth, &m_framebufferHeight);
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, &Application::onFramebufferSize);
    glfwSetCursorPosCallback(m_window, &Application::onCursorPosition);
    glfwSetScrollCallback(m_window, &Application::onScroll);

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE)
    {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(m_window, &cursorX, &cursorY);
    m_lastMouseX = cursorX;
    m_lastMouseY = cursorY;
    m_firstMouseEvent = true;

    // 天空球顶点深度被压到远平面，LEQUAL 才能让它在清空后的深度上通过测试。
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_MULTISAMPLE);

    std::cout << "[Application] OpenGL " << glGetString(GL_VERSION) << " | renderer: " << glGetString(GL_RENDERER)
              << '\n';
    std::cout << "[Application] window " << m_windowWidth << "x" << m_windowHeight << " | framebuffer "
              << m_framebufferWidth << "x" << m_framebufferHeight << '\n';
    return true;
}

bool Application::loadShaders()
{
    const std::string assetDirectory = m_assetDirectory.empty() ? RENDERINGCUBE_ASSET_DIR : m_assetDirectory;

    if (!m_shadowDepthShader.loadFromFiles(assetDirectory + "/shader/shadowDepth.vert",
                                           assetDirectory + "/shader/shadowDepth.frag"))
    {
        return false;
    }
    if (!m_pbrRenderer.initialize(assetDirectory))
    {
        return false;
    }
    if (!m_skyDome.initialize(assetDirectory))
    {
        return false;
    }
    if (!m_shadowMap.initialize(kShadowMapSize))
    {
        return false;
    }
    // 覆盖层是可降级图层：着色器或字体后端缺失时只关闭 UI，三维场景照常运行。
    if (!m_overlay.initialize(assetDirectory))
    {
        std::cerr << "[Application] overlay disabled (shader or font backend missing)\n";
    }

    // 太阳是可选图层：它自己的 shader 缺失时只禁用太阳，场景照常运行。
    if (!m_sun.initialize(assetDirectory))
    {
        std::cerr << "[Application] sun sprite disabled (shader missing)\n";
    }

    // 热重载：监视所有着色器及其 #include 依赖；改 .vert/.frag 后不需要重启进程。
    m_shaderWatcher.clear();
    m_shaderWatcher.add(m_pbrRenderer.shader());
    m_shaderWatcher.add(m_skyDome.shader());
    m_shaderWatcher.add(m_sun.shader());
    m_shaderWatcher.add(m_shadowDepthShader);
    m_shaderWatcher.add(m_overlay.shader());
    return true;
}

void Application::releaseResources()
{
    if (m_window == nullptr)
    {
        return;
    }

    // 顺序：先释放 GL 资源（上下文仍有效），再销毁窗口。
    m_jetTopTexture.reset();
    m_jetBottomTexture.reset();
    m_jetSideTexture.reset();
    m_sun.reset();
    m_overlay.reset();
    m_skyDome.reset();
    m_shadowMap.reset();
    m_shadowDepthShader.reset();
    m_pbrRenderer.reset();

    // 场景对象持有 Mesh（GL 资源），必须在上下文仍有效时销毁。
    m_scene.clear();
    m_dice = nullptr;
    m_ground = nullptr;
    m_helicopter = nullptr;
    m_jet = nullptr;
    m_jetFlying = false;
    m_driving = false;

    glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
    m_initialized = false;
}

// --- 场景 -----------------------------------------------------------------

void Application::createScene()
{
    if (m_jetOnly)
    {
        createGltfModel();
        updateInteractionPrompt();
        return;
    }

    // 地面：纯接收者，不写进阴影贴图（原因见 renderScene 的说明）。
    const PBRMaterial groundMaterial{glm::vec3(0.60f, 0.60f, 0.58f), 0.0f, 0.90f, 1.0f, 1.0f};
    m_ground = &m_scene.add<MeshObject>(MeshObject::createGroundMesh(kGroundHalfExtent), groundMaterial);
    m_ground->setCastsShadow(false);

    // 骰子：单位立方体 + 六面材质 + 点数 + 自转，全部封装在 DiceObject。
    if (!m_aircraftOnly)
    {
        m_dice = &m_scene.add<DiceObject>(DiceObject::createCubeMesh(), kCubeCenterHeight, kCubeScale,
                                          glm::radians(kCubeInitialSpinDegrees), kCubeSpinAxis);
    }

    // Reference-inspired attack helicopter; geometry is reusable independently of Scene.
    const auto attackModel = glm::scale(glm::translate(glm::mat4(1), glm::vec3(-2.8f, 1.0f, -1.4f)) *
                                            glm::rotate(glm::mat4(1), glm::radians(-24.0f), glm::vec3(0, 1, 0)),
                                        glm::vec3(.50f));
    const PBRMaterial attackMaterial{glm::vec3(1), .25f, .50f, 1.0f, 1.0f};
    // [载具创建] GL 上下文已建立；生成四份部件网格，再交给 Scene 持有载具对象。
    // 若增加另一种载具，从此处替换/追加几何和 SceneObject，不要在渲染器中添加机型判断。
    m_helicopter =
        &m_scene.add<AttackHelicopterObject>(AttackHelicopterGeometry::createMeshes(), attackMaterial, attackModel);

    // [glTF 模型] 可选的静态模型；加载失败只影响它自己。
    // --aircraft-only 是 AH-64D 近景检查模式，不再额外加载大模型。
    if (!m_aircraftOnly)
    {
        createGltfModel();
    }

    updateInteractionPrompt();
}

void Application::createGltfModel()
{
    if (m_modelPath.empty())
    {
        return;
    }

    const std::string assetDirectory = m_assetDirectory.empty() ? RENDERINGCUBE_ASSET_DIR : m_assetDirectory;
    std::filesystem::path path(m_modelPath);
    if (path.is_relative())
    {
        path = std::filesystem::path(assetDirectory) / path;
    }

    GltfAsset asset;
    std::string error;
    if (!loadGltf(path.string(), asset, error))
    {
        std::cerr << "[Application] glTF skipped: " << error << '\n';
        return;
    }

    if (asset.parts.empty())
    {
        std::cerr << "[Application] glTF has no geometry: " << path << '\n';
        return;
    }

    // 单块焊接网格按估算铰链区域拆成机身 + 控制面；拆件失败时退化为整机。
    Su57SplitData split = splitSu57(asset.parts.front());

    const glm::vec3 extent = asset.boundsMax - asset.boundsMin;
    const std::filesystem::path textureDirectory = std::filesystem::path(assetDirectory) / "texture";
    const bool hasLivery =
        m_jetTopTexture.loadBmp((textureDirectory / "su57_top.bmp").string()) &&
        m_jetBottomTexture.loadBmp((textureDirectory / "su57_bottom.bmp").string());
    const bool hasSideLivery = hasLivery && m_jetSideTexture.loadBmp((textureDirectory / "su57_side.bmp").string());
    m_jetLiveryLoaded = hasLivery;
    if (!hasLivery)
        std::cerr << "[Application] Su-57 livery textures unavailable; using procedural fallback\n";

    std::vector<FixedWingObject::Part> parts;
    const auto appendPart = [&parts, &asset, extent, hasLivery, hasSideLivery, this](
                                const GltfPartData& data, int surfaceIndex, const Su57SurfaceData* surface)
    {
        std::vector<glm::vec2> uvs(data.positions.size());
        if (extent.x > 1.0e-6f && extent.z > 1.0e-6f)
        {
            for (std::size_t i = 0; i < data.positions.size(); ++i)
            {
                const glm::vec3& position = data.positions[i];
                uvs[i] = glm::vec2((position.x - asset.boundsMin.x) / extent.x,
                                   (position.z - asset.boundsMin.z) / extent.z);
            }
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->upload(data.positions, data.colors, data.normals, uvs, data.indices);
        if (!mesh->valid())
            return;

        FixedWingObject::Part part;
        part.mesh = mesh;
        part.material = data.material;
        part.material.materialMode = 2;
        part.material.albedoTop = hasLivery ? &m_jetTopTexture : nullptr;
        part.material.albedoBottom = hasLivery ? &m_jetBottomTexture : nullptr;
        part.material.albedoSide = hasSideLivery ? &m_jetSideTexture : nullptr;
        part.material.albedo = glm::vec3(1.0f);
        part.material.metallic = 0.03f;
        part.material.roughness = 0.64f;
        part.nodeTransform = data.nodeTransform;
        if (surface != nullptr)
        {
            part.hingePoint = surface->hingePoint;
            part.hingeAxis = surface->hingeAxis;
            part.minAngle = surface->minAngle;
            part.maxAngle = surface->maxAngle;
            part.surfaceIndex = surfaceIndex;
        }
        parts.push_back(std::move(part));
    };

    appendPart(split.body, -1, nullptr);
    for (const Su57SurfaceData& surface : split.surfaces)
        appendPart(surface.geometry, surface.id - 1, &surface);

    if (parts.empty())
    {
        std::cerr << "[Application] glTF has no uploadable primitives: " << path << '\n';
        return;
    }

    // 模型原点在包围盒中心；物理需要绕重心旋转，所以先把模型旋到机体坐标 +X 机头，
    // 再减去重心，得到"机体坐标 -> 模型视觉坐标"的固定矩阵。
    constexpr float kModelScale = 3.6f;
    const glm::vec3 targetCenter(3.2f, 0.0f, -1.5f);
    const glm::vec3 sourceCenter = (asset.boundsMin + asset.boundsMax) * 0.5f;
    const glm::mat4 modelToBody = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 cgBody = glm::vec3(modelToBody * glm::vec4(sourceCenter, 1.0f));
    const glm::mat4 visualFromBody = glm::scale(glm::mat4(1.0f), glm::vec3(kModelScale)) *
                                     glm::translate(glm::mat4(1.0f), -cgBody) * modelToBody;

    const flight::FixedWingParameters wingParameters;
    flight::RigidBodyState initialState;
    initialState.position = glm::vec3(targetCenter.x / FixedWingObject::kUnitsPerMeter,
                                      wingParameters.gearHeight,
                                      targetCenter.z / FixedWingObject::kUnitsPerMeter);
    initialState.orientation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    initialState.grounded = true;

    const size_t partCount = parts.size();
    m_jet = &m_scene.add<FixedWingObject>(std::move(parts), visualFromBody, initialState);
    // 1.5M 三角形的模型也会进入阴影 pass；卡顿时可用 --no-shadow 关闭。
    m_jet->setCastsShadow(true);

    std::cout << "[Application] glTF model: " << path.string() << " (" << partCount << " parts, "
              << split.surfaces.size() << " control surfaces, " << asset.boundsMax.x - asset.boundsMin.x << " x "
              << asset.boundsMax.y - asset.boundsMin.y << " x " << asset.boundsMax.z - asset.boundsMin.z
              << " local units)\n";
}

void Application::drawItem(const DrawItem& item)
{
    if (item.mesh == nullptr)
    {
        return;
    }
    m_pbrRenderer.draw(*item.mesh, item.model, item.material, item.pipCount, item.firstIndex, item.indexCount);
}

// --- 输入 -----------------------------------------------------------------

void Application::processInput(float deltaSeconds)
{
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        return;
    }

    const auto axis = [this](int positive, int negative)
    {
        const float positiveValue = (glfwGetKey(m_window, positive) == GLFW_PRESS) ? 1.0f : 0.0f;
        const float negativeValue = (glfwGetKey(m_window, negative) == GLFW_PRESS) ? 1.0f : 0.0f;
        return positiveValue - negativeValue;
    };

    // [覆盖层] F1 切换调参面板；面板可见时释放光标，让鼠标去点控件。
    const bool uiToggleDown = glfwGetKey(m_window, GLFW_KEY_F1) == GLFW_PRESS;
    if (uiToggleDown && !m_uiToggleWasDown)
    {
        m_uiVisible = !m_uiVisible;
        if (m_uiVisible)
        {
            m_uiReleasedMouse = m_mouseLookEnabled;
            setMouseLookEnabled(false);
        }
        else
        {
            setMouseLookEnabled(m_uiReleasedMouse);
        }
    }
    m_uiToggleWasDown = uiToggleDown;

    if (m_uiVisible)
    {
        // 光标是窗口坐标，覆盖层用 framebuffer 像素；HiDPI 下两者不同，必须换算。
        double cursorX = 0.0;
        double cursorY = 0.0;
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetCursorPos(m_window, &cursorX, &cursorY);
        glfwGetWindowSize(m_window, &windowWidth, &windowHeight);
        const float scaleX = windowWidth > 0 ? static_cast<float>(m_framebufferWidth) / windowWidth : 1.0f;
        const float scaleY = windowHeight > 0 ? static_cast<float>(m_framebufferHeight) / windowHeight : 1.0f;
        const bool pressed = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        m_overlay.setMouse(static_cast<float>(cursorX) * scaleX, static_cast<float>(cursorY) * scaleY, pressed);
    }
    else
    {
        m_overlay.setMouse(-10000.0f, -10000.0f, false);
    }

    // [重置] 非飞行状态也可以把飞机放回初始停机位。
    if (!m_jetFlying)
    {
        const bool resetDown = glfwGetKey(m_window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
        if (resetDown && !m_jetResetWasDown && m_jet != nullptr)
        {
            resetJet();
            return;
        }
        m_jetResetWasDown = resetDown;
    }

    // [翼面检查] K 进入/退出；检查中 1-5 选择翼面组，[ ] 调整，0 归零。
    const bool surfaceToggleDown = glfwGetKey(m_window, GLFW_KEY_K) == GLFW_PRESS;
    if (surfaceToggleDown && !m_surfaceToggleWasDown)
        setSurfaceTest(!m_surfaceTest);
    m_surfaceToggleWasDown = surfaceToggleDown;

    if (m_surfaceTest)
    {
        updateSurfaceTest(deltaSeconds);
        const float forward = axis(GLFW_KEY_W, GLFW_KEY_S);
        const float right = axis(GLFW_KEY_D, GLFW_KEY_A);
        const float worldUp = axis(GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN);
        m_camera.moveLocal(forward, right, worldUp, deltaSeconds);
        return;
    }

    // [喷气机] J 切换驾驶；飞行中优先处理喷气机输入，避免和自由相机/直升机争用按键。
    const bool jetToggleDown = glfwGetKey(m_window, GLFW_KEY_J) == GLFW_PRESS;
    if (jetToggleDown && !m_jetToggleWasDown)
        setJetFlying(!m_jetFlying);
    m_jetToggleWasDown = jetToggleDown;

    if (m_jetFlying && m_jet)
    {
        // 飞行摇杆习惯：S 拉杆抬头，W 推杆低头；A/D 滚转，Q/E 偏航。
        const float pitch = axis(GLFW_KEY_S, GLFW_KEY_W);
        const float roll = axis(GLFW_KEY_D, GLFW_KEY_A);
        const float yaw = axis(GLFW_KEY_E, GLFW_KEY_Q);

        // B 切换机轮刹车；Backspace 回到初始停机状态。
        const bool brakeDown = glfwGetKey(m_window, GLFW_KEY_B) == GLFW_PRESS;
        if (brakeDown && !m_jetBrakeWasDown)
        {
            m_jetBrakes = !m_jetBrakes;
            m_jet->setBrakes(m_jetBrakes);
        }
        m_jetBrakeWasDown = brakeDown;

        const bool resetDown = glfwGetKey(m_window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
        if (resetDown && !m_jetResetWasDown)
        {
            resetJet();
            return;
        }
        m_jetResetWasDown = resetDown;

        const bool throttleUp = glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        const bool throttleDown = glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

        // T 切换自动油门；Shift/Ctrl 会切到手动油门并保持，直到再次按 T。
        const bool autoToggleDown = glfwGetKey(m_window, GLFW_KEY_T) == GLFW_PRESS;
        if (autoToggleDown && !m_jetAutoThrottleWasDown)
        {
            m_jetAutoThrottle = !m_jetAutoThrottle;
            if (!m_jetAutoThrottle)
                m_jetThrottle = m_jet->dynamics().throttle();
        }
        m_jetAutoThrottleWasDown = autoToggleDown;

        if (throttleUp || throttleDown)
        {
            if (m_jetAutoThrottle)
            {
                m_jetAutoThrottle = false;
                m_jetThrottle = m_jet->dynamics().throttle();
            }
            m_jetThrottle = std::clamp(m_jetThrottle +
                                           ((throttleUp ? 1.0f : 0.0f) - (throttleDown ? 1.0f : 0.0f)) * 0.6f *
                                               deltaSeconds,
                                       0.0f, 1.0f);
        }
        else if (m_jetAutoThrottle)
        {
            m_jetThrottle = m_jet->dynamics().throttle();
        }

        // P 切换自动驾驶；一旦有俯仰/滚转/偏航输入，自动退出自驾并保持手动姿态。
        const bool autopilotToggleDown = glfwGetKey(m_window, GLFW_KEY_P) == GLFW_PRESS;
        if (autopilotToggleDown && !m_jetAutopilotWasDown)
        {
            m_jetAutopilot = !m_jetAutopilot;
            m_jet->setAutopilot(m_jetAutopilot);
        }
        m_jetAutopilotWasDown = autopilotToggleDown;

        flight::FixedWingControls controls;
        controls.pitch = pitch;
        controls.roll = roll;
        controls.yaw = yaw;
        controls.throttle = m_jetThrottle;
        const bool manualAttitude = std::abs(pitch) > 0.01f || std::abs(roll) > 0.01f || std::abs(yaw) > 0.01f;
        if (manualAttitude && m_jetAutopilot)
        {
            m_jetAutopilot = false;
            m_jet->setAutopilot(false);
        }
        m_jet->setManualControls(controls);
        m_jet->setThrottleControl(!m_jetAutoThrottle, m_jetThrottle);
        return;
    }

    // [驾驶交互] G 仅在按下边沿触发；离得太远时按住 G 再靠近，也不会自动进入。
    const bool interactDown = glfwGetKey(m_window, GLFW_KEY_G) == GLFW_PRESS;
    if (interactDown && !m_interactWasDown)
        setDriving(!m_driving);
    m_interactWasDown = interactDown;

    float forward = axis(GLFW_KEY_W, GLFW_KEY_S);
    const float right = axis(GLFW_KEY_D, GLFW_KEY_A);
    const float worldUp = axis(GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN);

    if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        forward *= kFastMoveMultiplier;
    }

    // 空格切换自转：必须做边沿检测。直接 if (spaceDown) toggle() 会在按住的每一帧翻转。
    const bool spaceDown = (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS);
    if (spaceDown && !m_spaceWasDown && !m_driving)
    {
        toggleCubeSpin();
    }
    m_spaceWasDown = spaceDown;

    if (m_helicopter)
    {
        const bool rotorToggleDown = glfwGetKey(m_window, GLFW_KEY_R) == GLFW_PRESS;
        if (rotorToggleDown && !m_rotorToggleWasDown)
            m_helicopter->toggleRotors();
        m_rotorToggleWasDown = rotorToggleDown;
        if (m_driving)
        {
            // [驾驶输入] 机头航向只由 Q/E 积分；鼠标只改变观察轨道。
            const float pedals = axis(GLFW_KEY_E, GLFW_KEY_Q);
            // Mouse orbit is free-look only. E turns the nose right, which is
            // negative world-Y yaw for the aircraft's +X forward convention.
            m_pilotHeading = std::remainder(m_pilotHeading - glm::radians(pedals * 70 * deltaSeconds), 6.28318530718f);
            flight::FlightControls controls;
            const float speed = m_helicopter->dynamics().parameters().maxSpeed *
                                (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 1.6f : 1.0f);
            // Movement follows the aircraft's actual heading, not the free-look camera.
            controls.horizontalVelocity = m_helicopter->movementDirection(axis(GLFW_KEY_W, GLFW_KEY_S), right) * speed;
            // [驾驶输入] 升降独立于机头倾角/相机 pitch，避免抬头看就上升。
            controls.climb = std::clamp(worldUp + (spaceDown ? 1.0f : 0.0f) -
                                            (glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ? 1.0f : 0.0f),
                                        -1.0f, 1.0f);
            controls.heading = m_pilotHeading;
            controls.pedals = -pedals;
            controls.stabilatorTrim = axis(GLFW_KEY_C, GLFW_KEY_Z);
            m_helicopter->setFlightControls(controls);
        }
        else if (m_helicopter->flightPhysicsEnabled())
            m_helicopter->clearFlightControls();
        else
        {
            m_helicopter->setPedalInput(axis(GLFW_KEY_E, GLFW_KEY_Q));
            m_helicopter->setStabilatorInput(axis(GLFW_KEY_C, GLFW_KEY_Z));
        }
    }

    if (!m_driving)
        m_camera.moveLocal(forward, right, worldUp, deltaSeconds);
}

// --- 渲染 -----------------------------------------------------------------

void Application::renderScene()
{
    const glm::mat4 viewMatrix = m_camera.viewMatrix();
    const float aspectRatio = (m_framebufferHeight > 0)
                                ? static_cast<float>(m_framebufferWidth) / static_cast<float>(m_framebufferHeight)
                                : 1.0f;
    const glm::mat4 projectionMatrix =
        glm::perspective(glm::radians(kCameraFieldOfViewDegrees), aspectRatio, kCameraNearPlane, kCameraFarPlane);

    const glm::mat4 lightSpaceMatrix = m_lightRig.lightSpaceMatrix();
    const bool shadowsActive = m_shadowsEnabled && (m_shadowMap.depthTexture() != 0);

    // 场景只负责产出 DrawItem；下面两条 pass 只消费它，不认识骰子/地面。
    m_drawItems.clear();
    m_shadowItems.clear();
    m_scene.appendDrawItems(m_drawItems);
    m_scene.appendShadowItems(m_shadowItems);

    // ---- Pass 1: 深度 pass（光源视角）----
    if (shadowsActive)
    {
        m_shadowMap.bindForWriting();

        m_shadowDepthShader.use();
        m_shadowDepthShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);

        glDisable(GL_BLEND);
        // 只写投射者的深度：地面是纯接收者，写进来只会自己和自己比较，
        // 就必须靠 polygon offset 把阴影整体推开——那正是脚下"亮缝"的来源。
        // 闭合体按 DrawItem 的要求只画背面，避免受光面与自身比较产生 acne。
        for (const DrawItem& item : m_shadowItems)
        {
            if (item.mesh == nullptr)
            {
                continue;
            }

            const bool cullFront = (item.shadowCull == DrawItem::FaceCull::Front);
            if (cullFront)
            {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
            }

            m_shadowDepthShader.setMat4("uModel", item.model);
            item.mesh->draw();

            if (cullFront)
            {
                glCullFace(GL_BACK);
                glDisable(GL_CULL_FACE);
            }
        }

        m_shadowMap.unbind(m_sceneFramebuffer, static_cast<unsigned int>(m_framebufferWidth),
                           static_cast<unsigned int>(m_framebufferHeight));
        gl::checkError("renderScene: shadow pass");
    }

    // ---- Pass 2: 主 pass ----
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_skyDome.render(viewMatrix, projectionMatrix);

    // 太阳：位置由灯光方向决定，和阴影同源。
    m_sun.render(m_camera.position(), viewMatrix, projectionMatrix, m_lightRig.sunDirection());

    m_pbrRenderer.beginFrame(viewMatrix, projectionMatrix, m_camera.position(), m_lightRig.light(), lightSpaceMatrix,
                             shadowsActive ? m_shadowMap.depthTexture() : 0, shadowsActive, m_reflectionStrength,
                             // 投射者不透明度决定阴影深度；多个投射者取最不透明者。
                             m_scene.shadowCasterOpacity());

    // 不透明先画。
    for (const DrawItem& item : m_drawItems)
    {
        if (item.material.opacity >= 1.0f)
        {
            drawItem(item);
        }
    }

    // 半透明最后画：按到相机的距离从远到近，单物体时相对顺序不变。
    std::vector<const DrawItem*> transparent;
    for (const DrawItem& item : m_drawItems)
    {
        if (item.material.opacity < 1.0f)
        {
            transparent.push_back(&item);
        }
    }

    if (!transparent.empty())
    {
        const glm::vec3 cameraPosition = m_camera.position();
        std::stable_sort(transparent.begin(), transparent.end(),
                         [&cameraPosition](const DrawItem* lhs, const DrawItem* rhs)
                         {
                             const glm::vec3 lhsCenter(lhs->model[3]);
                             const glm::vec3 rhsCenter(rhs->model[3]);
                             return glm::distance(cameraPosition, lhsCenter) > glm::distance(cameraPosition, rhsCenter);
                         });

        // 标准 alpha 混合：透射近似，没有折射。深度写关闭，只画正面避免叠影。
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDepthMask(GL_FALSE);

        for (const DrawItem* item : transparent)
        {
            drawItem(*item);
        }

        glDepthMask(GL_TRUE);
        glCullFace(GL_FRONT);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }

    // [覆盖层] 所有三维物体绘制后叠加：速度药丸 + 可选调参面板。
    drawOverlay();
    gl::checkError("renderScene: main pass");
}

// --- 离屏截图 -------------------------------------------------------------

bool Application::captureFrame(const std::string& path, int width, int height)
{
    if (!m_initialized || width <= 0 || height <= 0)
    {
        return false;
    }

    // 先渲染到多重采样帧缓冲，再 blit 到单采样纹理读取像素。
    // 否则 --screenshot 出来的图没有 MSAA，与窗口里 4x 抗锯齿的观感不一致；
    // 用多重采样 renderbuffer 而不是纹理，是因为 GL 3.3 不能用纹理做 MSAA 附件。
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const GLsizei samples =
        static_cast<GLsizei>(std::max(1, std::min(kScreenshotSamples, static_cast<int>(maxSamples))));

    gl::Framebuffer multisampleFramebuffer{ gl::FramebufferDeleter{} };
    gl::Renderbuffer multisampleColor{ gl::RenderbufferDeleter{} };
    gl::Renderbuffer multisampleDepth{ gl::RenderbufferDeleter{} };
    gl::Framebuffer resolveFramebuffer{ gl::FramebufferDeleter{} };
    gl::Texture resolveColor{ gl::TextureDeleter{} };

    glGenFramebuffers(1, multisampleFramebuffer.address());
    glGenRenderbuffers(1, multisampleColor.address());
    glGenRenderbuffers(1, multisampleDepth.address());
    glGenFramebuffers(1, resolveFramebuffer.address());
    glGenTextures(1, resolveColor.address());

    glBindRenderbuffer(GL_RENDERBUFFER, multisampleColor.id());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, multisampleDepth.id());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, multisampleFramebuffer.id());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleColor.id());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleDepth.id());

    bool success = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (!success)
    {
        std::cerr << "[Application] multisampled offscreen framebuffer is incomplete\n";
    }

    if (success)
    {
        glBindTexture(GL_TEXTURE_2D, resolveColor.id());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, resolveFramebuffer.id());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveColor.id(), 0);

        success = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        if (!success)
        {
            std::cerr << "[Application] resolve framebuffer is incomplete\n";
        }
    }

    if (success)
    {
        // renderScene() 只依赖 m_framebufferWidth/Height 与 m_sceneFramebuffer，因此临时改写即可。
        const int savedWidth = m_framebufferWidth;
        const int savedHeight = m_framebufferHeight;
        const GLuint savedFramebuffer = m_sceneFramebuffer;
        m_framebufferWidth = width;
        m_framebufferHeight = height;
        m_sceneFramebuffer = multisampleFramebuffer.id();

        // 必须显式绑定：关闭阴影时 renderScene 不会走阴影 pass 的 unbind，
        // 否则会一直画在解析用的单采样 FBO 上，随后被空的多重采样内容覆盖。
        glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer);
        glViewport(0, 0, width, height);
        renderScene();
        gl::checkError("captureFrame: renderScene");

        glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFramebuffer.id());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFramebuffer.id());
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        gl::checkError("captureFrame: resolve blit");

        std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFramebuffer.id());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        gl::checkError("captureFrame: readPixels");

        m_framebufferWidth = savedWidth;
        m_framebufferHeight = savedHeight;
        m_sceneFramebuffer = savedFramebuffer;

        success = writeBmp24(path, width, height, pixels);
        if (!success)
        {
            std::cerr << "[Application] failed to write " << path << '\n';
        }
        else
        {
            std::cout << "[Application] wrote " << path << " (" << width << "x" << height << ", " << samples
                      << "x MSAA)\n";
        }
    }

    // 句柄在离开作用域时自动删除，这里只把默认帧缓冲和视口还原。
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer);
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    return success;
}

// --- 回调 -----------------------------------------------------------------

void Application::onFramebufferSize(GLFWwindow* window, int width, int height)
{
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application == nullptr)
    {
        return;
    }

    application->m_framebufferWidth = width;
    application->m_framebufferHeight = height;
    glViewport(0, 0, width, height);
}

void Application::onCursorPosition(GLFWwindow* window, double xPosition, double yPosition)
{
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application == nullptr)
    {
        return;
    }

    const double deltaX = xPosition - application->m_lastMouseX;
    const double deltaY = yPosition - application->m_lastMouseY;
    application->m_lastMouseX = xPosition;
    application->m_lastMouseY = yPosition;

    if (!application->m_mouseLookEnabled)
    {
        return;
    }

    // 第一帧的增量是未定义的大跳变，丢弃。
    if (application->m_firstMouseEvent)
    {
        application->m_firstMouseEvent = false;
        return;
    }

    // [第三人称相机] 驾驶/喷气机只更新观察轨道，不修改飞行指令。
    if (application->m_jetFlying || application->m_driving)
        application->m_chaseCamera.mouseMovement(static_cast<float>(deltaX), static_cast<float>(deltaY));
    else
        application->m_camera.processMouseMovement(static_cast<float>(deltaX), static_cast<float>(deltaY));
}

void Application::onScroll(GLFWwindow* window, double xOffset, double yOffset)
{
    (void)xOffset;

    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application == nullptr)
    {
        return;
    }

    // 滚轮：只累加目标高度，由 Camera::updateElevation() 每帧平滑逼近。
    // [第三人称相机] 同一个滚轮输入按模式分流：驾驶缩放，自由观察升降。
    if (application->m_jetFlying || application->m_driving)
        application->m_chaseCamera.zoom(static_cast<float>(yOffset));
    else
        application->m_camera.addElevation(static_cast<float>(yOffset) * kWheelElevationStep);
}


// --- 覆盖层 ---------------------------------------------------------------

void Application::setLightAngles(float azimuthDegrees, float elevationDegrees)
{
    m_lightAzimuthDegrees = azimuthDegrees;
    m_lightElevationDegrees = elevationDegrees;

    const float azimuth = glm::radians(azimuthDegrees);
    const float elevation = glm::radians(elevationDegrees);
    const glm::vec3 sunDirection(std::cos(elevation) * std::cos(azimuth), std::sin(elevation),
                                 std::cos(elevation) * std::sin(azimuth));
    // LightRig 的方向是“光的传播方向”，所以太阳方向要取反。
    m_lightRig.setDirection(-sunDirection);
}

void Application::drawOverlay()
{
    if (!m_overlay.shader().valid() || m_framebufferWidth <= 0 || m_framebufferHeight <= 0)
    {
        return;
    }

    m_overlay.beginFrame(m_framebufferWidth, m_framebufferHeight);

    // 速度药丸：喷气机飞行时显示喷气机速度，否则沿用直升机/静态显示。
    const float speedKmh =
        (m_jetFlying && m_jet)
            ? m_jet->speedKmh()
            : (m_helicopter && m_helicopter->flightPhysicsEnabled()
                   ? glm::length(m_helicopter->dynamics().state().velocity) * 3.6f
                   : 0.0f);
    char speedText[128] = {};
    if (m_jetFlying && m_jet)
    {
        std::snprintf(speedText, sizeof(speedText), "速度 %.1f km/h · 油门 %.0f%% %s · 过载 %.1fg %s", speedKmh,
                      m_jet->dynamics().throttle() * 100.0f, m_jetAutoThrottle ? "自动" : "手动",
                      m_jet->dynamics().loadFactor(), m_jetAutopilot ? "自驾开" : "自驾关");
    }
    else
    {
        std::snprintf(speedText, sizeof(speedText), "速度 %.1f km/h", speedKmh);
    }
    const float pillScale = 1.2f;
    const glm::vec2 pillSize(m_overlay.textWidth(speedText, pillScale) + 28.0f,
                             m_overlay.lineHeight() * pillScale + 16.0f);
    const glm::vec2 pillPosition(static_cast<float>(m_framebufferWidth) - pillSize.x - 24.0f, 24.0f);
    m_overlay.fillRect(pillPosition, pillSize, glm::vec4(0.05f, 0.06f, 0.09f, 0.80f), 10.0f);
    m_overlay.text(pillPosition + glm::vec2(14.0f, 8.0f), speedText, glm::vec4(0.70f, 0.93f, 1.0f, 1.0f), pillScale);

    // [翼面检查] 列表：选中项高亮，所有角度实时显示。
    if (m_surfaceTest)
    {
        const float panelWidth = 440.0f;
        const float lineHeight = m_overlay.lineHeight();
        const glm::vec2 panelPosition(static_cast<float>(m_framebufferWidth) - panelWidth - 24.0f,
                                      24.0f + pillSize.y + 12.0f);
        const glm::vec2 panelSize(panelWidth, lineHeight * 8.0f + 20.0f);
        m_overlay.fillRect(panelPosition, panelSize, glm::vec4(0.05f, 0.06f, 0.09f, 0.85f), 10.0f);

        const glm::vec4 selectedColor(0.70f, 0.93f, 1.0f, 1.0f);
        const glm::vec4 normalColor(0.82f, 0.84f, 0.86f, 0.85f);
        const char* names[5] = {"flaperon", "前缘襟翼", "全动平尾", "方向舵", "喷口"};
        char line[128] = {};
        m_overlay.text(panelPosition + glm::vec2(14.0f, 8.0f), "翼面检查（K 退出）", selectedColor, 1.0f);
        for (int i = 0; i < 5; ++i)
        {
            std::snprintf(line, sizeof(line), "%d %s  %+6.1f°", i + 1, names[i], m_surfaceTestAngles[i]);
            m_overlay.text(panelPosition + glm::vec2(14.0f, 8.0f + static_cast<float>(i + 1) * lineHeight), line,
                           i == m_surfaceGroup ? selectedColor : normalColor, 1.0f);
        }
        m_overlay.text(panelPosition + glm::vec2(14.0f, 8.0f + 6.0f * lineHeight),
                       "←/→ 或 [ ] 调整（点按 +2°）", normalColor, 0.9f);
        m_overlay.text(panelPosition + glm::vec2(14.0f, 8.0f + 7.0f * lineHeight), "0 归零", normalColor, 0.9f);
    }

    // [飞行 HUD] 左下角仪表：高度/升降/航向/迎角/接地/刹车/自驾/状态。
    bool stallWarning = false;
    if (m_jetFlying && m_jet)
    {
        const flight::RigidBodyState& state = m_jet->dynamics().state();
        const flight::FixedWingParameters& parameters = m_jet->dynamics().parameters();
        const glm::vec3 forward = m_jet->forwardDirection();
        float heading = glm::degrees(std::atan2(forward.z, forward.x));
        if (heading < 0.0f)
            heading += 360.0f;
        const float alphaDegrees = glm::degrees(m_jet->dynamics().angleOfAttack());
        const float stallDegrees = glm::degrees(parameters.stallAngle);
        stallWarning = std::fabs(alphaDegrees) > stallDegrees * 0.9f;

        const float lineHeight = m_overlay.lineHeight();
        const glm::vec2 panelSize(300.0f, lineHeight * 9.0f + 20.0f);
        const glm::vec2 panelPosition(24.0f, static_cast<float>(m_framebufferHeight) - panelSize.y - 48.0f);
        m_overlay.fillRect(panelPosition, panelSize, glm::vec4(0.05f, 0.06f, 0.09f, 0.80f), 10.0f);

        const glm::vec4 textColor(0.82f, 0.86f, 0.90f, 0.95f);
        const glm::vec4 warnColor(1.0f, 0.42f, 0.35f, 1.0f);
        const auto row = [&](int index, const char* text, const glm::vec4& color)
        {
            m_overlay.text(panelPosition + glm::vec2(14.0f, 8.0f + static_cast<float>(index) * lineHeight), text, color,
                           1.0f);
        };

        char line[128] = {};
        std::snprintf(line, sizeof(line), "高度 %.0f m", state.position.y);
        row(0, line, textColor);
        std::snprintf(line, sizeof(line), "升降 %+.1f m/s", state.velocity.y);
        row(1, line, textColor);
        std::snprintf(line, sizeof(line), "航向 %.0f°", heading);
        row(2, line, textColor);
        std::snprintf(line, sizeof(line), "迎角 %+.1f°", alphaDegrees);
        row(3, line, stallWarning ? warnColor : textColor);
        std::snprintf(line, sizeof(line), "过载 %.1fg", m_jet->dynamics().loadFactor());
        row(4, line, textColor);
        if (m_jetHasTouchedDown)
            std::snprintf(line, sizeof(line), "接地 %+.1f m/s", m_jetTouchdownSpeed);
        else
            std::snprintf(line, sizeof(line), "接地 --");
        row(5, line, m_jetHardLanding ? warnColor : textColor);
        std::snprintf(line, sizeof(line), "刹车 %s", m_jetBrakes ? "开" : "关");
        row(6, line, textColor);
        std::snprintf(line, sizeof(line), "自驾 %s", m_jetAutopilot ? "开" : "关");
        row(7, line, textColor);
        std::snprintf(line, sizeof(line), "状态 %s", state.grounded ? (m_jetHardLanding ? "地面·重着陆" : "地面") : "空中");
        row(8, line, m_jetHardLanding ? warnColor : textColor);
    }

    if (stallWarning && std::fmod(glfwGetTime(), 0.6) < 0.4)
    {
        const char* warning = "STALL";
        const float scale = 2.0f;
        const float width = m_overlay.textWidth(warning, scale);
        m_overlay.text(glm::vec2((static_cast<float>(m_framebufferWidth) - width) * 0.5f, 34.0f), warning,
                       glm::vec4(1.0f, 0.25f, 0.20f, 1.0f), scale);
    }

    if (!m_uiVisible)
    {
        m_overlay.text(glm::vec2(24.0f, static_cast<float>(m_framebufferHeight) - m_overlay.lineHeight() - 18.0f),
                       "F1 调参面板", glm::vec4(1.0f, 1.0f, 1.0f, 0.40f), 1.0f);
        m_overlay.endFrame();
        return;
    }

    // 行数固定，先按行数算高度，保证面板底色正好包住内容。
    constexpr int kPanelRows = 17;
    const float headerHeight = 10.0f + m_overlay.lineHeight() * 1.1f + 12.0f;
    const float panelWidth = 320.0f;
    const float panelHeight = headerHeight + static_cast<float>(kPanelRows) * m_overlay.rowHeight() + 16.0f;
    m_overlay.beginPanel(glm::vec2(24.0f, 24.0f), panelWidth, panelHeight, "调试面板");

    char buffer[128] = {};
    const float frameMilliseconds = std::max(0.01f, m_smoothedFrameMilliseconds);
    std::snprintf(buffer, sizeof(buffer), "%.1f FPS · %.2f ms", 1000.0f / frameMilliseconds, frameMilliseconds);
    m_overlay.valueRow("帧率", buffer);

    int triangles = 0;
    for (const DrawItem& item : m_drawItems)
    {
        if (item.mesh != nullptr)
        {
            const int count = item.indexCount > 0 ? item.indexCount : static_cast<int>(item.mesh->indexCount());
            triangles += count / 3;
        }
    }
    std::snprintf(buffer, sizeof(buffer), "%d 项 · %d 三角形", static_cast<int>(m_drawItems.size()), triangles);
    m_overlay.valueRow("绘制", buffer);

    m_overlay.section("画面");
    bool shadows = m_shadowsEnabled;
    if (m_overlay.checkboxRow("阴影", shadows))
    {
        enableShadows(shadows);
    }

    float reflection = m_reflectionStrength;
    if (m_overlay.sliderRow("反射强度", reflection, 0.0f, 1.0f, "%.2f"))
    {
        setReflectionStrength(reflection);
    }

    float opacity = objectOpacity();
    if (m_overlay.sliderRow("不透明度", opacity, 0.05f, 1.0f, "%.2f"))
    {
        setObjectOpacity(opacity);
    }

    const PBRMaterial firstFace = cubeFaceMaterial(0);
    float roughness = firstFace.roughness;
    if (m_overlay.sliderRow("粗糙度", roughness, 0.04f, 1.0f, "%.2f"))
    {
        for (int face = 0; face < kCubeFaceCount; ++face)
        {
            PBRMaterial material = cubeFaceMaterial(face);
            material.roughness = roughness;
            setCubeFaceMaterial(face, material);
        }
    }

    float metallic = firstFace.metallic;
    if (m_overlay.sliderRow("金属度", metallic, 0.0f, 1.0f, "%.2f"))
    {
        for (int face = 0; face < kCubeFaceCount; ++face)
        {
            PBRMaterial material = cubeFaceMaterial(face);
            material.metallic = metallic;
            setCubeFaceMaterial(face, material);
        }
    }

    bool pips = pipsEnabled();
    if (m_overlay.checkboxRow("骰子点数", pips))
    {
        setPipsEnabled(pips);
    }

    bool spin = cubeSpinEnabled();
    if (m_overlay.checkboxRow("自转", spin) && spin != cubeSpinEnabled())
    {
        toggleCubeSpin();
    }

    m_overlay.section("光照");
    float azimuth = m_lightAzimuthDegrees;
    if (m_overlay.sliderRow("方位", azimuth, -180.0f, 180.0f, "%.0f°"))
    {
        setLightAngles(azimuth, m_lightElevationDegrees);
    }
    float elevation = m_lightElevationDegrees;
    if (m_overlay.sliderRow("高度", elevation, -10.0f, 89.0f, "%.0f°"))
    {
        setLightAngles(m_lightAzimuthDegrees, elevation);
    }

    m_overlay.section("载具");
    bool rotors = m_rotorsEnabled;
    if (m_overlay.checkboxRow("旋翼", rotors))
    {
        m_rotorsEnabled = rotors;
        setHelicopterRotorsEnabled(rotors);
    }

    char speedLine[64] = {};
    std::snprintf(speedLine, sizeof(speedLine), "%.1f km/h", speedKmh);
    m_overlay.valueRow("速度", speedLine);
    if (m_jetFlying)
        m_overlay.valueRow("驾驶", "喷气机（J 退出）");
    else
        m_overlay.valueRow("驾驶", m_driving ? "直升机（G 退出）" : "G 靠近后进入");

    m_overlay.endPanel();
    m_overlay.endFrame();
}
void Application::setOverlayPanelVisible(bool visible)
{
    m_uiVisible = visible;
    if (visible && m_window != nullptr)
    {
        m_uiReleasedMouse = m_mouseLookEnabled;
        setMouseLookEnabled(false);
    }
}
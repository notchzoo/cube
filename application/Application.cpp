#include "Application.h"
#include "AttackHelicopterGeometry.h"
#include "AttackHelicopterObject.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
// --- 场景与相机常量 -------------------------------------------------------
constexpr int kShadowMapSize = 2048;
constexpr float kCameraFieldOfViewDegrees = 50.0f;
constexpr float kCameraNearPlane = 0.1f;
constexpr float kCameraFarPlane = 4000.0f;
constexpr float kGroundHalfExtent = 1200.0f;
constexpr float kCubeScale = 0.8f;              // 单位立方体 -> 边长 1.6
constexpr float kCubeCenterHeight = 1.95f;      // 中心高度；底面 = 该值 - kCubeScale
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
    if (width <= 0 || height <= 0 ||
        rgbaPixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4u)
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
    putU32(26, 0x00180001u);   // planes = 1, bits per pixel = 24
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
        const unsigned char* source =
            rgbaPixels.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
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
    : m_title(title != nullptr ? title : "RenderingCube")
    , m_windowWidth(width > 0 ? width : 1400)
    , m_windowHeight(height > 0 ? height : 900)
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

    const double started=glfwGetTime();
    while (glfwWindowShouldClose(m_window) == GLFW_FALSE)
    {
        const double now = glfwGetTime();
        float deltaSeconds = static_cast<float>(now - m_lastFrameTime);
        m_lastFrameTime = now;
        // 断点/拖动窗口会让 delta 变得很大，夹紧避免相机瞬移。
        deltaSeconds = std::clamp(deltaSeconds, 0.0f, kMaxFrameDelta);

        // [载具更新] 顺序固定：采集驾驶输入 -> 飞机物理/动画 -> 跟随相机 -> 渲染。
        // 不要在 renderScene 的两个 pass 中推进物理或动画，否则一帧会积分两次。
        processInput(deltaSeconds);
        // 滚轮只累加目标高度，这里每帧平滑逼近，效果和按键一样连续。
        if(!m_driving) m_camera.updateElevation(deltaSeconds);

        // 自转由对象自己推进：暂停时角度保留，恢复从原处继续。
        if (m_dice != nullptr)
        {
            m_dice->update(deltaSeconds);
        }
        if (m_helicopter != nullptr) m_helicopter->update(deltaSeconds);
        updateVehicleCamera(deltaSeconds);

        renderScene();

        glfwSwapBuffers(m_window);
        glfwPollEvents();
        if (durationSeconds>0 && glfwGetTime()-started>=durationSeconds) break;
    }
}

void Application::setHelicopterRotorsEnabled(bool enabled)
{
    if (m_helicopter) m_helicopter->setRotorsEnabled(enabled);
}

void Application::setHelicopterPose(float mainRotor,float tailRotor,float tailPitch,float stabilator)
{
    if (!m_helicopter) return;
    m_helicopter->setRotorAngles(mainRotor,tailRotor);
    m_helicopter->setControlAngles(tailPitch,stabilator);
}

// [驾驶交互] G 与 --drive 共用的唯一切换入口；距离检查必须先于任何驾驶状态修改。
// 进入：检查座舱距离 -> 启用物理 -> 记录机头航向 -> 初始化第三人称相机。
// 退出：清空驾驶输入，保留飞机物理状态，让自由相机留在当前观察位置。
void Application::setDriving(bool enabled)
{
    if(!m_helicopter || m_driving==enabled) return;
    if(enabled && !m_helicopter->canEnterFrom(m_camera.position()))
    {
        updateInteractionPrompt();
        std::cout<<"[Driving] out of reach; approach the cockpit before pressing G\n";
        return;
    }
    m_driving=enabled;
    m_helicopter->clearFlightControls();
    m_firstMouseEvent=true;m_titleUpdateTime=0;
    if(enabled)
    {
        m_helicopter->enableFlightPhysics();
        const auto aircraftForward=m_helicopter->forwardDirection();
        m_pilotHeading=std::atan2(-aircraftForward.z,aircraftForward.x);
        m_chaseCamera.enter(m_helicopter->focusPoint(),aircraftForward,m_camera);
        std::cout<<"[Driving] entered; WASD move, Space/Ctrl climb, Q/E yaw, G exit\n";
    }
    else
    {
        // Leave the free camera at its current safe chase-camera position.
        m_camera.setPosition(m_camera.position());
        updateInteractionPrompt();
        std::cout<<"[Driving] exited; free camera restored\n";
    }
}

// [驾驶交互] 自由观察时的标题提示；锚点和距离读取当前飞机 Model，因此会跟随飞机移动。
void Application::updateInteractionPrompt()
{
    if(!m_window || !m_helicopter || m_driving) return;
    std::string prompt=m_title;
    if(m_helicopter->canEnterFrom(m_camera.position())) prompt+=" | 按 G 驾驶直升机";
    else
    {
        const float unitsPerMeter=glm::length(glm::vec3(m_helicopter->modelMatrix()[0]))/2;
        const float metres=glm::distance(m_camera.position(),m_helicopter->interactionPoint())/std::max(.0001f,unitsPerMeter);
        std::ostringstream distance;distance<<std::fixed<<std::setprecision(1)<<metres;
        prompt+=" | 靠近座舱后按 G 驾驶 | 距离 "+distance.str()+" m";
    }
    glfwSetWindowTitle(m_window,prompt.c_str());
}

// [第三人称相机] run 在飞机 update 之后调用；此处跟随最新机体位置，并调整阴影覆盖范围。
// 自由模式只更新交互提示，不把自由相机拉回飞机。
void Application::updateVehicleCamera(float dt)
{
    if(!m_helicopter) return;
    if(m_helicopter->flightPhysicsEnabled())
    {
        const auto position=glm::vec3(m_helicopter->modelMatrix()[3]);
        const auto direction=m_lightRig.normalizedDirection();
        const auto shadow=position+direction*(position.y/std::max(.15f,-direction.y));
        m_lightRig.setShadowFocus((position+shadow)*.5f,std::max(9.0f,glm::distance(position,shadow)*.6f+5));
    }
    m_titleUpdateTime-=dt;
    if(!m_driving)
    {
        if(m_titleUpdateTime<=0) {updateInteractionPrompt();m_titleUpdateTime=.25f;}
        return;
    }
    m_chaseCamera.update(m_helicopter->focusPoint(),dt,m_camera);
    if(m_titleUpdateTime<=0)
    {
        const auto& state=m_helicopter->dynamics().state();
        const std::string title="驾驶 | WASD移动 空格升/Ctrl降 Q/E转向 G退出 | "+
            std::to_string(static_cast<int>(glm::length(state.velocity)*3.6f))+" km/h | 高度 "+
            std::to_string(static_cast<int>(state.position.y))+" m";
        glfwSetWindowTitle(m_window,title.c_str());m_titleUpdateTime=.25f;
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

    std::cout << "[Application] OpenGL " << glGetString(GL_VERSION) << " | renderer: "
              << glGetString(GL_RENDERER) << '\n';
    std::cout << "[Application] window " << m_windowWidth << "x" << m_windowHeight
              << " | framebuffer " << m_framebufferWidth << "x" << m_framebufferHeight << '\n';
    return true;
}

bool Application::loadShaders()
{
    const std::string assetDirectory = RENDERINGCUBE_ASSET_DIR;

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
    if(!m_speedometer.initialize(assetDirectory))
    {
        std::cerr<<"[Application] speed display initialization failed\n";
        return false;
    }

    // 太阳是可选图层：它自己的 shader 缺失时只禁用太阳，场景照常运行。
    if (!m_sun.initialize(assetDirectory))
    {
        std::cerr << "[Application] sun sprite disabled (shader missing)\n";
    }
    return true;
}

void Application::releaseResources()
{
    if (m_window == nullptr)
    {
        return;
    }

    // 顺序：先释放 GL 资源（上下文仍有效），再销毁窗口。
    m_sun.reset();
    m_speedometer.reset();
    m_skyDome.reset();
    m_shadowMap.reset();
    m_shadowDepthShader.reset();
    m_pbrRenderer.reset();

    // 场景对象持有 Mesh（GL 资源），必须在上下文仍有效时销毁。
    m_scene.clear();
    m_dice = nullptr;
    m_ground = nullptr;
    m_helicopter = nullptr;
    m_driving=false;

    glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
    m_initialized = false;
}

// --- 场景 -----------------------------------------------------------------

void Application::createScene()
{
    // 地面：纯接收者，不写进阴影贴图（原因见 renderScene 的说明）。
    const PBRMaterial groundMaterial{ glm::vec3(0.60f, 0.60f, 0.58f), 0.0f, 0.90f, 1.0f, 1.0f };
    m_ground = &m_scene.add<MeshObject>(MeshObject::createGroundMesh(kGroundHalfExtent), groundMaterial);
    m_ground->setCastsShadow(false);

    // 骰子：单位立方体 + 六面材质 + 点数 + 自转，全部封装在 DiceObject。
    if (!m_aircraftOnly)
    {
        m_dice = &m_scene.add<DiceObject>(DiceObject::createCubeMesh(),
                                      kCubeCenterHeight,
                                      kCubeScale,
                                      glm::radians(kCubeInitialSpinDegrees),
                                      kCubeSpinAxis);
    }

    // Reference-inspired attack helicopter; geometry is reusable independently of Scene.
    const auto attackModel = glm::scale(
        glm::translate(glm::mat4(1), glm::vec3(-2.8f, 1.0f, -1.4f)) *
        glm::rotate(glm::mat4(1), glm::radians(-24.0f), glm::vec3(0, 1, 0)), glm::vec3(.50f));
    const PBRMaterial attackMaterial{glm::vec3(1), .25f, .50f, 1.0f, 1.0f};
    // [载具创建] GL 上下文已建立；生成四份部件网格，再交给 Scene 持有载具对象。
    // 若增加另一种载具，从此处替换/追加几何和 SceneObject，不要在渲染器中添加机型判断。
    m_helicopter = &m_scene.add<AttackHelicopterObject>(AttackHelicopterGeometry::createMeshes(),attackMaterial,attackModel);
    updateInteractionPrompt();
}

void Application::drawItem(const DrawItem& item)
{
    if (item.mesh == nullptr)
    {
        return;
    }
    m_pbrRenderer.draw(*item.mesh,
                       item.model,
                       item.material,
                       item.pipCount,
                       item.firstIndex,
                       item.indexCount);
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

    // [驾驶交互] G 仅在按下边沿触发；离得太远时按住 G 再靠近，也不会自动进入。
    const bool interactDown=glfwGetKey(m_window,GLFW_KEY_G)==GLFW_PRESS;
    if(interactDown && !m_interactWasDown) setDriving(!m_driving);
    m_interactWasDown=interactDown;

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
        const bool rotorToggleDown=glfwGetKey(m_window,GLFW_KEY_R)==GLFW_PRESS;
        if (rotorToggleDown && !m_rotorToggleWasDown) m_helicopter->toggleRotors();
        m_rotorToggleWasDown=rotorToggleDown;
        if(m_driving)
        {
            // [驾驶输入] 机头航向只由 Q/E 积分；鼠标只改变观察轨道。
            const float pedals=axis(GLFW_KEY_E,GLFW_KEY_Q);
            // Mouse orbit is free-look only. E turns the nose right, which is
            // negative world-Y yaw for the aircraft's +X forward convention.
            m_pilotHeading=std::remainder(m_pilotHeading-glm::radians(pedals*70*deltaSeconds),6.28318530718f);
            flight::FlightControls controls;
            const float speed=m_helicopter->dynamics().parameters().maxSpeed*
                (glfwGetKey(m_window,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS?1.6f:1.0f);
            // Movement follows the aircraft's actual heading, not the free-look camera.
            controls.horizontalVelocity=m_helicopter->movementDirection(axis(GLFW_KEY_W,GLFW_KEY_S),right)*speed;
            // [驾驶输入] 升降独立于机头倾角/相机 pitch，避免抬头看就上升。
            controls.climb=std::clamp(worldUp+(spaceDown?1.0f:0.0f)-
                (glfwGetKey(m_window,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS?1.0f:0.0f),-1.0f,1.0f);
            controls.heading=m_pilotHeading;controls.pedals=-pedals;
            controls.stabilatorTrim=axis(GLFW_KEY_C,GLFW_KEY_Z);
            m_helicopter->setFlightControls(controls);
        }
        else if(m_helicopter->flightPhysicsEnabled()) m_helicopter->clearFlightControls();
        else
        {
            m_helicopter->setPedalInput(axis(GLFW_KEY_E,GLFW_KEY_Q));
            m_helicopter->setStabilatorInput(axis(GLFW_KEY_C,GLFW_KEY_Z));
        }
    }

    if(!m_driving) m_camera.moveLocal(forward, right, worldUp, deltaSeconds);
}

// --- 渲染 -----------------------------------------------------------------

void Application::renderScene()
{
    const glm::mat4 viewMatrix = m_camera.viewMatrix();
    const float aspectRatio = (m_framebufferHeight > 0)
        ? static_cast<float>(m_framebufferWidth) / static_cast<float>(m_framebufferHeight)
        : 1.0f;
    const glm::mat4 projectionMatrix = glm::perspective(glm::radians(kCameraFieldOfViewDegrees),
                                                        aspectRatio,
                                                        kCameraNearPlane,
                                                        kCameraFarPlane);

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

        m_shadowMap.unbind(m_sceneFramebuffer,
                           static_cast<unsigned int>(m_framebufferWidth),
                           static_cast<unsigned int>(m_framebufferHeight));
    }

    // ---- Pass 2: 主 pass ----
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_skyDome.render(viewMatrix, projectionMatrix);

    // 太阳：位置由灯光方向决定，和阴影同源。
    m_sun.render(m_camera.position(), viewMatrix, projectionMatrix, m_lightRig.sunDirection());

    m_pbrRenderer.beginFrame(viewMatrix,
                             projectionMatrix,
                             m_camera.position(),
                             m_lightRig.light(),
                             lightSpaceMatrix,
                             shadowsActive ? m_shadowMap.depthTexture() : 0,
                             shadowsActive,
                             m_reflectionStrength,
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
                             return glm::distance(cameraPosition, lhsCenter) >
                                    glm::distance(cameraPosition, rhsCenter);
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

    // [速度显示] 所有三维物体绘制后叠加；读取飞机真实三维线速度，m/s 转为 km/h。
    const float speedKmh=m_helicopter && m_helicopter->flightPhysicsEnabled()
        ?glm::length(m_helicopter->dynamics().state().velocity)*3.6f:0.0f;
    m_speedometer.render(m_framebufferWidth,m_framebufferHeight,speedKmh);
}

// --- 离屏截图 -------------------------------------------------------------

bool Application::captureFrame(const std::string& path, int width, int height)
{
    if (!m_initialized || width <= 0 || height <= 0)
    {
        return false;
    }

    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthBuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &colorTexture);
    glGenRenderbuffers(1, &depthBuffer);

    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    bool success = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (!success)
    {
        std::cerr << "[Application] offscreen framebuffer is incomplete\n";
    }
    else
    {
        // renderScene() 只依赖 m_framebufferWidth/Height 与 m_sceneFramebuffer，因此临时改写即可。
        const int savedWidth = m_framebufferWidth;
        const int savedHeight = m_framebufferHeight;
        const GLuint savedFramebuffer = m_sceneFramebuffer;
        m_framebufferWidth = width;
        m_framebufferHeight = height;
        m_sceneFramebuffer = framebuffer;

        glViewport(0, 0, width, height);
        renderScene();

        std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

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
            std::cout << "[Application] wrote " << path << " (" << width << "x" << height << ")\n";
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer);
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    glDeleteRenderbuffers(1, &depthBuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteFramebuffers(1, &framebuffer);
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

    // [第三人称相机] 驾驶只更新观察轨道，不修改 m_pilotHeading 或飞行指令。
    if(application->m_driving)
        application->m_chaseCamera.mouseMovement(static_cast<float>(deltaX),static_cast<float>(deltaY));
    else application->m_camera.processMouseMovement(static_cast<float>(deltaX), static_cast<float>(deltaY));
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
    if(application->m_driving) application->m_chaseCamera.zoom(static_cast<float>(yOffset));
    else application->m_camera.addElevation(static_cast<float>(yOffset) * kWheelElevationStep);
}

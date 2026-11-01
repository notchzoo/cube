#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Camera.h"
#include "ThirdPersonCamera.h"
#include "DiceObject.h"
#include "LightRig.h"
#include "MeshObject.h"
#include "PBRMaterial.h"
#include "PBRRenderer.h"
#include "Scene.h"
#include "Shader.h"
#include "ShaderWatcher.h"
#include "ShadowMap.h"
#include "SkyDome.h"
#include "SunSprite.h"
#include "ui/Overlay.h"

struct GLFWwindow;
class AttackHelicopterObject;

/// 程序骨架：窗口、输入、每帧调度与渲染顺序。
///
/// 职责边界（P0/P1 重构后）：
///   * 场景内容归 Scene / DiceObject / MeshObject，Application 不再知道"骰子有六个面"；
///   * 灯光归 LightRig，光照几何（光源正交矩阵、太阳方向）不再散落；
///   * 本体只做编排：调 Scene 产出 DrawItem，再交给 PBRRenderer / 阴影着色器消费。
class Application
{
public:
    Application(int width = 1400, int height = 900, const char* title = "RenderingCube");
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// 建窗口、加载 Shader、创建场景。任一步失败都会清理已建资源并返回 false。
    bool initialize();

    /// 阻塞式主循环，直到窗口被关闭。
    void run(float durationSeconds = 0.0f);

    // --- 场景配置：必须在 initialize() 之后调用（对象在那时创建）------------
    void setDirectionalLight(const DirectionalLight& light);
    void setLightDirection(const glm::vec3& direction);

    /// 一次性把六面都设成同一材质（等价于"纯色立方体"）。
    void setCubeMaterial(const PBRMaterial& material);

    /// 单独设置某一个面的材质；face 顺序 0=+Z, 1=-Z, 2=+X, 3=-X, 4=+Y, 5=-Y。
    void setCubeFaceMaterial(int face, const PBRMaterial& material);

    void setGroundMaterial(const PBRMaterial& material);

    /// 骰子点数（1..6，按面顺序）是否绘制。关闭后每面只剩材质颜色。
    void setPipsEnabled(bool enabled);

    /// 读取某个面当前的材质（用于在原有颜色上批量改 roughness / metallic）。
    PBRMaterial cubeFaceMaterial(int face) const;

    /// 面顺序：0=+Z, 1=-Z, 2=+X, 3=-X, 4=+Y, 5=-Y。
    static constexpr int kCubeFaceCount = DiceObject::kFaceCount;

    /// 环境反射强度，夹紧到 [0, 1]。
    void setReflectionStrength(float strength);

    // --- 立方体自转 ---------------------------------------------------------
    // 角度是状态，时间只是推进它的输入；暂停时角度保留，恢复从原处继续。
    void setCubeSpinEnabled(bool enabled);
    void toggleCubeSpin();
    void setCubeSpinSpeed(float radiansPerSecond);
    void setCubeSpinAngle(float radians);
    bool cubeSpinEnabled() const;
    float cubeSpinAngle() const;
    float cubeSpinSpeed() const;

    /// 立方体不透明度，夹紧到 [0, 1]；小于 1 时按半透明路径绘制。
    void setObjectOpacity(float opacity);

    void enableShadows(bool enabled);

    // --- 太阳（独立图层，可整体移除）----------------------------------------
    void setSunStyle(const SunStyle& style);
    const SunStyle& sunStyle() const;
    bool setSunTexture(const std::string& path);
    void clearSunTexture();

    /// 仅用于截图/调试：直接摆放相机（yaw / pitch 单位为度）。
    void setCameraPose(const glm::vec3& position, float yawDegrees, float pitchDegrees);

    /// 关闭后释放鼠标并忽略视角增量；便于截图核对或演示。
    void setMouseLookEnabled(bool enabled);

    /// 资产目录（内含 shader/、texture/）。必须在 initialize() 之前调用；
    /// 不设置时退回编译期记录的项目内路径。
    void setAssetDirectory(const std::string& directory);

    /// initialize() 之后调用：启动时就打开调参面板（配合 --ui / 截图用）。
    void setOverlayPanelVisible(bool visible);

    /// 必须在 initialize() 之前调用；隐藏窗口时仍可正常渲染。
    void setWindowVisible(bool visible);
    /// Before initialize(): omit the dice for unobstructed aircraft close-up inspection.
    void setAircraftOnly(bool enabled) { m_aircraftOnly = enabled; }
    void setHelicopterRotorsEnabled(bool enabled);
    /// Deterministic articulation pose, in degrees; also useful for screenshots.
    void setHelicopterPose(float mainRotor, float tailRotor, float tailPitch, float stabilator);
    /// [驾驶交互] G/启动参数共用入口；进入需要座舱距离检查，退出不受距离限制。
    void setDriving(bool enabled);
    bool driving() const { return m_driving; }

    /// 把当前场景渲染到离屏帧缓冲并写成 24 位 BMP（不含窗口，可重复复现）。
    bool captureFrame(const std::string& path, int width, int height);

    bool shadowsEnabled() const { return m_shadowsEnabled; }
    bool mouseLookEnabled() const { return m_mouseLookEnabled; }
    float reflectionStrength() const { return m_reflectionStrength; }
    float objectOpacity() const;
    bool pipsEnabled() const;
    int framebufferWidth() const { return m_framebufferWidth; }
    int framebufferHeight() const { return m_framebufferHeight; }
    size_t sceneObjectCount() const { return m_scene.objectCount(); }

private:
    bool createWindow();
    bool loadShaders();
    void createScene();

    void processInput(float deltaSeconds);
    void renderScene();
    void releaseResources();
    void updateVehicleCamera(float deltaSeconds); // 飞机更新后调用，跟随相机和阴影
    void updateInteractionPrompt();               // 自由模式显示靠近距离/按 G 提示
    void drawOverlay();                           // 速度药丸 + 调参面板（所有三维绘制之后）
    void setLightAngles(float azimuthDegrees, float elevationDegrees);

    /// 按一个 DrawItem 设置材质并绘制；主体与阴影 pass 都不认识具体对象类型。
    void drawItem(const DrawItem& item);

    static void onFramebufferSize(GLFWwindow* window, int width, int height);
    static void onCursorPosition(GLFWwindow* window, double xPosition, double yPosition);
    static void onScroll(GLFWwindow* window, double xOffset, double yOffset);

    GLFWwindow* m_window = nullptr;
    std::string m_title;
    std::string m_assetDirectory;
    int m_windowWidth = 1400;
    int m_windowHeight = 900;
    int m_framebufferWidth = 0;
    int m_framebufferHeight = 0;

    Camera m_camera;
    ThirdPersonCamera m_chaseCamera;
    LightRig m_lightRig;
    Scene m_scene;

    // 场景对象的非拥有指针：所有权在 Scene，这里只用于转发配置接口。
    DiceObject* m_dice = nullptr;
    MeshObject* m_ground = nullptr;
    AttackHelicopterObject* m_helicopter = nullptr;

    PBRRenderer m_pbrRenderer;
    ShadowMap m_shadowMap;
    SkyDome m_skyDome;
    SunSprite m_sun;
    Overlay m_overlay; // [覆盖层] 速度显示 + 调参面板（替代旧的 Win32 GDI 速度面板）
    bool m_uiVisible = false;        // F1 切换面板；可见时释放光标给面板交互
    bool m_uiReleasedMouse = false;  // 打开面板前的鼠标捕获状态，关闭时恢复
    bool m_uiToggleWasDown = false;
    bool m_rotorsEnabled = true;     // 面板里切换旋翼时同步到载具对象
    float m_smoothedFrameMilliseconds = 16.0f;
    float m_lightAzimuthDegrees = 37.4f;   // 与默认光向 (-0.55, -1, -0.42) 对应的太阳方位
    float m_lightElevationDegrees = 57.2f; // 同上，太阳高度角
    Shader m_shadowDepthShader;
    ShaderWatcher m_shaderWatcher; // [热重载] 监视全部着色器及其 #include 依赖

    // 每帧复用的绘制项缓冲，避免逐帧分配。
    std::vector<DrawItem> m_drawItems;
    std::vector<DrawItem> m_shadowItems;

    float m_reflectionStrength = 0.55f;
    bool m_shadowsEnabled = true;
    bool m_mouseLookEnabled = true;
    bool m_windowVisible = true;
    bool m_aircraftOnly = false;
    bool m_spaceWasDown = false;
    bool m_rotorToggleWasDown = false;
    bool m_driving = false, m_interactWasDown = false; // 驾驶状态 / G 上一帧是否按住
    float m_pilotHeading = 0;                          // 飞机目标航向，弧度；与鼠标观察角度独立
    float m_titleUpdateTime = 0;                       // 窗口标题刷新倒计时，避免每帧调用系统接口

    // 当前场景渲染目标：窗口时为 0，离屏截图时指向临时 FBO。
    GLuint m_sceneFramebuffer = 0;

    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_firstMouseEvent = true;

    double m_lastFrameTime = 0.0;
    bool m_initialized = false;
};

#pragma once

#include <array>
#include <string>

#include <glm/glm.hpp>

#include "Camera.h"
#include "DirectionalLight.h"
#include "Mesh.h"
#include "PBRMaterial.h"
#include "PBRRenderer.h"
#include "Shader.h"
#include "ShadowMap.h"
#include "SkyDome.h"
#include "SunSprite.h"

struct GLFWwindow;

/// 拥有 GLFW 生命周期、输入、场景对象、渲染顺序与对外配置接口。
/// 这里不出现任何裸 GL 缓冲区创建：几何归 Mesh，PBR uniform 归 PBRRenderer，
/// 深度贴图归 ShadowMap，天空球归 SkyDome。
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
    void run();

    // --- 对外配置：常见视觉调整不需要改 PBRRenderer 或 GLSL ---
    void setDirectionalLight(const DirectionalLight& light);
    void setLightDirection(const glm::vec3& direction);

    /// 一次性把六面都设成同一材质（等价于"纯色立方体"）。
    void setCubeMaterial(const PBRMaterial& material);

    /// 单独设置某一个面的材质；face 顺序与 createCubeGeometry() 的面法线一致：
    /// 0=+Z, 1=-Z, 2=+X, 3=-X, 4=+Y, 5=-Y。
    void setCubeFaceMaterial(int face, const PBRMaterial& material);

    void setGroundMaterial(const PBRMaterial& material);

    /// 骰子点数（1..6，按面顺序）是否绘制。关闭后每面只剩材质颜色。
    void setPipsEnabled(bool enabled);

    /// 读取某个面当前的材质（用于在原有颜色上批量改 roughness / metallic）。
    PBRMaterial cubeFaceMaterial(int face) const;

    /// 面顺序：0=+Z, 1=-Z, 2=+X, 3=-X, 4=+Y, 5=-Y。
    static constexpr int kCubeFaceCount = 6;

    /// 环境反射强度，夹紧到 [0, 1]。
    void setReflectionStrength(float strength);

    // --- 立方体自转 ---------------------------------------------------------
    // 设计要点：角度是状态，时间只是推进它的输入。
    //   * 暂停 = 不再累加，角度保持不变 → 恢复时从原位置继续，不会跳变；
    //   * 速度、角度都可以随时改，改完立刻生效；
    //   * 交互时用空格切换（边沿触发），脚本/截图时用 setCubeSpinAngle() 定死角度。
    void setCubeSpinEnabled(bool enabled);
    void toggleCubeSpin();
    void setCubeSpinSpeed(float radiansPerSecond);
    /// 直接设定当前角度（弧度）。截图前调用即可得到逐像素可复现的画面。
    void setCubeSpinAngle(float radians);
    bool cubeSpinEnabled() const { return m_spinEnabled; }
    float cubeSpinAngle() const { return m_spinAngle; }
    float cubeSpinSpeed() const { return m_spinSpeed; }

    /// 立方体不透明度，夹紧到 [0, 1]；小于 1 时按半透明路径绘制。
    void setObjectOpacity(float opacity);

    void enableShadows(bool enabled);

    // --- 太阳（独立图层，可整体移除）----------------------------------------
    // 太阳位置由光方向决定（sunDir = -light.direction），不引入新的场景状态。
    // 换"材质"只需 setSunStyle()，或 setSunTexture() 给一张 BMP（亮度当 alpha）。
    void setSunStyle(const SunStyle& style);
    const SunStyle& sunStyle() const;
    bool setSunTexture(const std::string& path);
    void clearSunTexture();

    /// 仅用于截图/调试：直接摆放相机（yaw / pitch 单位为度）。
    void setCameraPose(const glm::vec3& position, float yawDegrees, float pitchDegrees);

    /// 关闭后释放鼠标并忽略视角增量；便于截图核对或演示。
    void setMouseLookEnabled(bool enabled);

    /// 必须在 initialize() 之前调用；隐藏窗口时仍可正常渲染。
    void setWindowVisible(bool visible);

    /// 把当前场景渲染到离屏帧缓冲并写成 24 位 BMP（不含窗口，可重复复现）。
    /// 用于无交互的视觉核对；返回 false 表示帧缓冲不完整或文件写入失败。
    bool captureFrame(const std::string& path, int width, int height);

    bool shadowsEnabled() const { return m_shadowsEnabled; }
    bool mouseLookEnabled() const { return m_mouseLookEnabled; }
    float reflectionStrength() const { return m_reflectionStrength; }
    float objectOpacity() const { return m_cubeOpacity; }
    bool pipsEnabled() const { return m_pipsEnabled; }
    int framebufferWidth() const { return m_framebufferWidth; }
    int framebufferHeight() const { return m_framebufferHeight; }

private:
    bool createWindow();
    bool loadShaders();
    void createScene();
    void createCubeGeometry();
    void createGroundGeometry();

    void processInput(float deltaSeconds);
    void renderScene();
    void releaseResources();

    /// 每帧根据 m_spinAngle 重建立方体的模型矩阵（平移 + 自转 + 缩放）。
    /// 阴影 pass 与主 pass 共用这一个矩阵，所以本体和阴影永远同步。
    void updateCubeModel();

    /// 把角度规整到 [0, 2π)，避免长时间运行后浮点精度变粗。
    void wrapSpinAngle();

    static void onFramebufferSize(GLFWwindow* window, int width, int height);
    static void onCursorPosition(GLFWwindow* window, double xPosition, double yPosition);
    static void onScroll(GLFWwindow* window, double xOffset, double yOffset);

    GLFWwindow* m_window = nullptr;
    std::string m_title;
    int m_windowWidth = 1400;
    int m_windowHeight = 900;
    int m_framebufferWidth = 0;
    int m_framebufferHeight = 0;

    Camera m_camera;
    DirectionalLight m_light;

    // 骰子：六个面各自一套材质 + 点数，面顺序 0=+Z, 1=-Z, 2=+X, 3=-X, 4=+Y, 5=-Y。
    // 地面 roughness 过低会出现一块被截断的巨大白色高光，所以保持 0.90。
    std::array<PBRMaterial, kCubeFaceCount> m_cubeFaceMaterials;
    std::array<int, kCubeFaceCount> m_cubeFacePips{ 1, 2, 3, 4, 5, 6 };
    float m_cubeOpacity = 1.0f;
    bool m_pipsEnabled = true;
    PBRMaterial m_groundMaterial{ glm::vec3(0.60f, 0.60f, 0.58f), 0.0f, 0.90f, 1.0f, 1.0f };

    // 自转状态（全部私有：外部只能通过上面的 setter 改）：
    //   m_spinAngle  累计角度，暂停时保持不变 —— 这是"停"的唯一依据；
    //   m_spinSpeed  弧度/秒，暂停期间仍保留，恢复时沿用；
    //   m_spinEnabled 是否正在推进；
    //   m_spaceWasDown 空格上一次的按下状态，用来做边沿检测（否则按住会每帧翻转）。
    bool m_spinEnabled = true;
    float m_spinAngle = 0.0f;
    float m_spinSpeed = 0.6f;
    bool m_spaceWasDown = false;

    Mesh m_cubeMesh;
    Mesh m_groundMesh;

    PBRRenderer m_pbrRenderer;
    ShadowMap m_shadowMap;
    SkyDome m_skyDome;
    SunSprite m_sun;
    Shader m_shadowDepthShader;

    glm::mat4 m_cubeModel{ 1.0f };
    glm::mat4 m_groundModel{ 1.0f };
    glm::mat4 m_lightSpaceMatrix{ 1.0f };

    float m_reflectionStrength = 0.55f;
    bool m_shadowsEnabled = true;
    bool m_mouseLookEnabled = true;
    bool m_windowVisible = true;

    // 当前场景渲染目标：窗口时为 0，离屏截图时指向临时 FBO。
    GLuint m_sceneFramebuffer = 0;

    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_firstMouseEvent = true;

    double m_lastFrameTime = 0.0;
    bool m_initialized = false;
};
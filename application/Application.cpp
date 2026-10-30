#include "Application.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
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
constexpr float kCubeCenterHeight = 1.95f;     // 底面离地 5mm，阴影贴合地面
constexpr int kCubeIndicesPerFace = 6;

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

    // 几何体是单位立方体，位置、旋转与尺寸全部由 Model 矩阵给出；这样顶点着色器
    // 里的对象空间坐标正好落在 [-1, 1]，骰子点数按面归一化时不需要额外 uniform。
    // 初始角度取 28°：让 +Z / +X / +Y 三个面同时可见（点数 1 / 3 / 5）。
    m_spinAngle = glm::radians(28.0f);
    updateCubeModel();

    m_groundModel = glm::mat4(1.0f);

    // 骰子六面：颜色、粗糙度、金属度各不相同，方便一眼看出"每面一套材质"。
    const std::array<PBRMaterial, kCubeFaceCount> defaultFaces = { {
        { glm::vec3(0.86f, 0.20f, 0.16f), 0.0f, 0.35f, 1.0f, 1.0f },  // +Z 红
        { glm::vec3(0.20f, 0.62f, 0.30f), 0.0f, 0.55f, 1.0f, 1.0f },  // -Z 绿
        { glm::vec3(0.18f, 0.38f, 0.84f), 0.0f, 0.22f, 1.0f, 1.0f },  // +X 蓝
        { glm::vec3(0.93f, 0.70f, 0.18f), 0.0f, 0.45f, 1.0f, 1.0f },  // -X 琥珀
        { glm::vec3(0.88f, 0.88f, 0.86f), 0.0f, 0.12f, 1.0f, 1.0f },  // +Y 米白（高光最锐）
        { glm::vec3(0.55f, 0.30f, 0.78f), 0.9f, 0.28f, 1.0f, 1.0f },  // -Y 紫色金属
    } };
    m_cubeFaceMaterials = defaultFaces;
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

void Application::run()
{
    if (!m_initialized)
    {
        return;
    }

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE)
    {
        const double now = glfwGetTime();
        float deltaSeconds = static_cast<float>(now - m_lastFrameTime);
        m_lastFrameTime = now;
        // 断点/拖动窗口会让 delta 变得很大，夹紧避免相机瞬移。
        deltaSeconds = std::clamp(deltaSeconds, 0.0f, kMaxFrameDelta);

        processInput(deltaSeconds);
        // 滚轮只累加目标高度，这里每帧平滑逼近，效果和按键一样连续。
        m_camera.updateElevation(deltaSeconds);

        // 自转推进：只有启用时才累加角度。暂停时角度停在原处，
        // 恢复后从同一角度继续，因此不会出现跳变。
        if (m_spinEnabled)
        {
            m_spinAngle += m_spinSpeed * deltaSeconds;
            wrapSpinAngle();
        }

        renderScene();

        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}

// --- 配置接口 -------------------------------------------------------------

void Application::setDirectionalLight(const DirectionalLight& light)
{
    m_light = light;
}

void Application::setLightDirection(const glm::vec3& direction)
{
    const float length = glm::length(direction);
    if (length < 1e-5f)
    {
        std::cerr << "[Application] light direction is almost zero, ignored\n";
        return;
    }
    m_light.direction = direction / length;
}

void Application::setCubeMaterial(const PBRMaterial& material)
{
    const PBRMaterial sanitized = material.clamped();
    for (PBRMaterial& face : m_cubeFaceMaterials)
    {
        face = sanitized;
    }
}

void Application::setCubeFaceMaterial(int face, const PBRMaterial& material)
{
    if (face < 0 || face >= kCubeFaceCount)
    {
        std::cerr << "[Application] cube face index out of range: " << face << '\n';
        return;
    }
    m_cubeFaceMaterials[static_cast<size_t>(face)] = material.clamped();
}

PBRMaterial Application::cubeFaceMaterial(int face) const
{
    if (face < 0 || face >= kCubeFaceCount)
    {
        std::cerr << "[Application] cube face index out of range: " << face << '\n';
        return PBRMaterial{};
    }
    return m_cubeFaceMaterials[static_cast<size_t>(face)];
}

void Application::setGroundMaterial(const PBRMaterial& material)
{
    m_groundMaterial = material.clamped();
}

void Application::setPipsEnabled(bool enabled)
{
    m_pipsEnabled = enabled;
}

void Application::setReflectionStrength(float strength)
{
    m_reflectionStrength = std::clamp(strength, 0.0f, 1.0f);
}

void Application::setObjectOpacity(float opacity)
{
    m_cubeOpacity = std::clamp(opacity, 0.0f, 1.0f);
}

void Application::enableShadows(bool enabled)
{
    m_shadowsEnabled = enabled;
}

// --- 自转 -----------------------------------------------------------------

void Application::setCubeSpinEnabled(bool enabled)
{
    m_spinEnabled = enabled;
}

void Application::toggleCubeSpin()
{
    m_spinEnabled = !m_spinEnabled;
}

void Application::setCubeSpinSpeed(float radiansPerSecond)
{
    // 允许负值（反向旋转），只把离谱的量级挡掉，避免浮点溢出成 NaN。
    m_spinSpeed = std::clamp(radiansPerSecond, -20.0f, 20.0f);
}

void Application::setCubeSpinAngle(float radians)
{
    m_spinAngle = radians;
    wrapSpinAngle();
    // 立即反映到模型矩阵上，保证截图/下一帧前状态一致。
    updateCubeModel();
}

void Application::wrapSpinAngle()
{
    constexpr float kTwoPi = 6.28318530717958647692f;
    m_spinAngle = std::fmod(m_spinAngle, kTwoPi);
    if (m_spinAngle < 0.0f)
    {
        m_spinAngle += kTwoPi;
    }
}

void Application::updateCubeModel()
{
    // 顺序：先缩放（单位立方体 -> 边长 1.6），再自转，最后抬到离地高度。
    // 这里只改模型矩阵，着色器完全不需要知道"时间"或"角度"。
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f),
                                                 glm::vec3(0.0f, kCubeCenterHeight, 0.0f));
    const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), m_spinAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(kCubeScale));
    m_cubeModel = translation * rotation * scale;
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
    return true;
}

void Application::releaseResources()
{
    if (m_window == nullptr)
    {
        return;
    }

    // 顺序：先释放 GL 资源（上下文仍有效），再销毁窗口。
    m_skyDome.reset();
    m_shadowMap.reset();
    m_shadowDepthShader.reset();
    m_pbrRenderer.reset();
    m_groundMesh.reset();
    m_cubeMesh.reset();

    glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
    m_initialized = false;
}

// --- 场景几何 -------------------------------------------------------------

void Application::createScene()
{
    createCubeGeometry();
    createGroundGeometry();
}

void Application::createCubeGeometry()
{
    // 硬边立方体：每个面 4 个独立顶点，法线在面内恒定，共 24 顶点 36 索引。
    const glm::vec3 faceNormals[6] = {
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
    };

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
    positions.reserve(24);
    colors.reserve(24);
    normals.reserve(24);
    indices.reserve(36);

    for (const glm::vec3& normal : faceNormals)
    {
        // 为每个面构造切向/副切向：tangent x bitangent = normal，保证绕序一致。
        const glm::vec3 tangent = (std::abs(normal.y) > 0.9f)
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), normal));
        const glm::vec3 bitangent = glm::cross(normal, tangent);

        const unsigned int base = static_cast<unsigned int>(positions.size());
        const glm::vec3 corners[4] = {
            -tangent - bitangent,
            tangent - bitangent,
            tangent + bitangent,
            -tangent + bitangent,
        };

        for (const glm::vec3& corner : corners)
        {
            // Offset along the face normal: without it every face would lie in
            // the plane through the cube center and the six quads would intersect.
            // Unit cube: object-space coordinates stay in [-1, 1], so the shader
            // can lay out dice pips without knowing the model scale.
            positions.push_back(corner + normal);
            normals.push_back(normal);
            // 固定 Mesh 布局包含颜色属性；PBR 反照率来自材质，这里填白色。
            colors.push_back(glm::vec3(1.0f));
        }

        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    m_cubeMesh.upload(positions, colors, normals, indices);
}

void Application::createGroundGeometry()
{
    const float extent = kGroundHalfExtent;
    const std::vector<glm::vec3> positions = {
        glm::vec3(-extent, 0.0f, -extent),
        glm::vec3(-extent, 0.0f, extent),
        glm::vec3(extent, 0.0f, extent),
        glm::vec3(extent, 0.0f, -extent),
    };
    const std::vector<glm::vec3> normals(4, glm::vec3(0.0f, 1.0f, 0.0f));
    const std::vector<glm::vec3> colors(4, glm::vec3(1.0f));
    const std::vector<unsigned int> indices = { 0, 1, 2, 0, 2, 3 };

    m_groundMesh.upload(positions, colors, normals, indices);
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

    float forward = axis(GLFW_KEY_W, GLFW_KEY_S);
    const float right = axis(GLFW_KEY_D, GLFW_KEY_A);
    const float worldUp = axis(GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN);

    if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        forward *= kFastMoveMultiplier;
    }

    // 空格切换自转：必须做边沿检测。直接 if (spaceDown) toggle() 会在按住的每一帧翻转。
    const bool spaceDown = (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS);
    if (spaceDown && !m_spaceWasDown)
    {
        toggleCubeSpin();
    }
    m_spaceWasDown = spaceDown;

    m_camera.moveLocal(forward, right, worldUp, deltaSeconds);
}

// --- 渲染 -----------------------------------------------------------------

void Application::renderScene()
{
    // 自转角度 -> 模型矩阵。放在所有 pass 之前，保证阴影 pass 与主 pass 用同一份变换。
    updateCubeModel();

    const glm::mat4 viewMatrix = m_camera.viewMatrix();
    const float aspectRatio = (m_framebufferHeight > 0)
        ? static_cast<float>(m_framebufferWidth) / static_cast<float>(m_framebufferHeight)
        : 1.0f;
    const glm::mat4 projectionMatrix = glm::perspective(glm::radians(kCameraFieldOfViewDegrees),
                                                        aspectRatio,
                                                        kCameraNearPlane,
                                                        kCameraFarPlane);

    const glm::vec3 lightDirection = m_light.normalizedDirection();
    m_lightSpaceMatrix = m_shadowMap.lightSpaceMatrix(lightDirection);
    const bool shadowsActive = m_shadowsEnabled && (m_shadowMap.depthTexture() != 0);

    // ---- Pass 1: 深度 pass（光源视角）----
    if (shadowsActive)
    {
        m_shadowMap.bindForWriting();

        m_shadowDepthShader.use();
        m_shadowDepthShader.setMat4("uLightSpaceMatrix", m_lightSpaceMatrix);

        glDisable(GL_BLEND);
        // 深度 pass 只写投射者（立方体）的深度。
        // 地面是纯接收者：把它写进来只会自己和自己做深度比较，就必须靠 polygon offset
        // 把阴影整体推开才能消 acne —— 而那一推正是立方体脚下那圈"亮缝"的来源。
        // 不写地面，既没有地面自阴影，也不需要任何 offset，阴影就能贴住接触点。

        // 立方体是闭合体：只渲染背面写深度，受光面就不会和自己做深度比较（消除 acne）。
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        m_shadowDepthShader.setMat4("uModel", m_cubeModel);
        m_cubeMesh.draw();
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);

        m_shadowMap.unbind(m_sceneFramebuffer,
                           static_cast<unsigned int>(m_framebufferWidth),
                           static_cast<unsigned int>(m_framebufferHeight));
    }

    // ---- Pass 2: 主 pass ----
    glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_skyDome.render(viewMatrix, projectionMatrix);

    m_pbrRenderer.beginFrame(viewMatrix,
                             projectionMatrix,
                             m_camera.position(),
                             m_light,
                             m_lightSpaceMatrix,
                             shadowsActive ? m_shadowMap.depthTexture() : 0,
                             shadowsActive,
                             m_reflectionStrength,
                             // 投射者（立方体）的不透明度决定阴影有多深，
                             // 与接收者自身材质无关。
                             m_cubeOpacity);

    // 不透明几何先画：地面。
    m_pbrRenderer.draw(m_groundMesh, m_groundModel, m_groundMaterial);

    // 立方体：一个面一次 draw，材质与点数各不相同（骰子）。
    const bool cubeTransparent = m_cubeOpacity < 1.0f;
    if (cubeTransparent)
    {
        // 标准 alpha 混合：透射近似，没有折射。
        // depth write 保持关闭，但只画正面：深度缓冲此时只有地面的深度，
        // 背面若也绘制会与正面叠加成鬼影（能"看穿"到立方体底面）。
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDepthMask(GL_FALSE);
    }

    for (int face = 0; face < kCubeFaceCount; ++face)
    {
        PBRMaterial faceMaterial = m_cubeFaceMaterials[static_cast<size_t>(face)];
        // 全局不透明度与每个面自己的不透明度相乘。
        faceMaterial.opacity = std::clamp(faceMaterial.opacity * m_cubeOpacity, 0.0f, 1.0f);
        const int pipCount = m_pipsEnabled ? m_cubeFacePips[static_cast<size_t>(face)] : 0;

        m_pbrRenderer.draw(m_cubeMesh,
                           m_cubeModel,
                           faceMaterial,
                           pipCount,
                           face * kCubeIndicesPerFace,
                           kCubeIndicesPerFace);
    }

    if (cubeTransparent)
    {
        glDepthMask(GL_TRUE);
        glCullFace(GL_FRONT);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
    }
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
    application->m_camera.addElevation(static_cast<float>(yOffset) * kWheelElevationStep);
}
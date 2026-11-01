#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "Application.h"
#include "AssetPath.h"

#ifndef RENDERINGCUBE_VERSION
#define RENDERINGCUBE_VERSION "dev"
#endif

namespace
{
struct Options
{
    float opacity = 1.0f;
    bool shadows = true;
    float reflectionStrength = 0.55f;
    bool mouseLook = true;
    bool hidden = false;
    bool aircraftOnly = false;
    bool drive = false; // [驾驶交互] --drive 只尝试进入；仍受座舱距离限制，需配合 --camera
    bool rotors = true;
    float mainRotorAngle = 0.0f;
    float tailRotorAngle = 0.0f;
    float tailPitch = 0.0f;
    float stabilatorAngle = 0.0f;
    float runFor = 0.0f;
    bool pips = true;
    bool spin = true;
    float spinAngleDegrees = -1.0f; // 负数表示“用 Application 的默认角度”
    bool hasSunRadiance = false;
    float sunRadiance = 0.0f;
    std::string sunTexturePath;
    bool hasCameraPose = false;
    glm::vec3 cameraPosition{0.0f, 3.0f, 6.6f};
    float cameraYawDegrees = -90.0f;
    float cameraPitchDegrees = -20.0f;
    std::string screenshotPath;
    bool overrideCubeMaterial = false;
    float cubeRoughness = 0.28f;
    float cubeMetallic = 0.0f;
    bool overlayPanel = false;
    bool showHelp = false;
    bool showVersion = false;
};

void printUsage()
{
    std::cout << R"(RenderingCube — OpenGL 3.3 Core PBR 场景

用法: RenderingCube [选项]

画面:
  --transparent              立方体不透明度 0.62
  --opacity <0..1>           指定不透明度
  --roughness <value>        覆盖六面粗糙度（保留每面颜色）
  --metallic <0..1>          覆盖六面金属度（保留每面颜色）
  --no-pips                  不画骰子点数，只显示每面材质颜色
  --no-shadow                关闭阴影 pass
  --no-reflection            关闭环境反射
  --camera <x y z yaw pitch> 摆放相机（yaw / pitch 单位为度）
  --sun <radiance>           太阳辐照度
  --sun-texture <file.bmp>   太阳贴图（亮度当 alpha）

载具:
  --aircraft-only            只显示飞机和地面，便于近距离检查机头
  --drive                    启动时尝试驾驶（需配合 --camera 靠近座舱）
  --no-rotors                启动时暂停旋翼
  --main-rotor-angle <度>    主旋翼初始相位
  --tail-rotor-angle <度>    尾桨初始相位
  --tail-pitch <度>          尾桨初始变距（夹紧到 ±18°）
  --stabilator-angle <度>    水平尾翼初始偏角（夹紧到 ±20°）

运行:
  --run-for <秒>             运行指定时长后退出
  --screenshot <file.bmp>    离屏渲染一帧（4x MSAA）后退出
  --hidden                   隐藏窗口（仍可渲染）
  --no-mouse                 释放鼠标并忽略视角输入
  --ui                       启动时就打开调参面板（F1 切换）
  --no-spin                  启动时不自转
  --spin-angle <度>          固定自转角度，用于截图逐像素复现

  --help, -h                 显示本帮助
  --version                  显示版本

未知选项或缺失参数会报错并以退出码 2 结束，不再静默忽略。
)";
}

/// 数字解析失败必须报错：静默退化成 0 会让截图/回归跑出“看起来正常”的错配置。
bool parseFloat(const char* text, float& out)
{
    if (text == nullptr || text[0] == '\0')
    {
        return false;
    }
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (end == text || *end != '\0' || !std::isfinite(value))
    {
        return false;
    }
    out = static_cast<float>(value);
    return true;
}
}

int main(int argc, char** argv)
{
    Options options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string name = argv[i];

        // 取一个数值参数；失败时立刻返回 false，由调用点决定退出。
        const auto takeFloat = [&](float& target)
        {
            if (i + 1 >= argc)
            {
                std::cerr << "选项 " << name << " 缺少参数\n用 --help 查看用法。\n";
                return false;
            }
            if (!parseFloat(argv[i + 1], target))
            {
                std::cerr << "选项 " << name << " 需要数字，实际收到: " << argv[i + 1] << '\n';
                return false;
            }
            ++i;
            return true;
        };

        const auto takeString = [&](std::string& target)
        {
            if (i + 1 >= argc)
            {
                std::cerr << "选项 " << name << " 缺少参数\n用 --help 查看用法。\n";
                return false;
            }
            target = argv[i + 1];
            ++i;
            return true;
        };

        if (name == "--help" || name == "-h")
        {
            options.showHelp = true;
        }
        else if (name == "--version")
        {
            options.showVersion = true;
        }
        else if (name == "--transparent")
        {
            options.opacity = 0.62f;
        }
        else if (name == "--no-shadow")
        {
            options.shadows = false;
        }
        else if (name == "--no-reflection")
        {
            options.reflectionStrength = 0.0f;
        }
        else if (name == "--no-mouse")
        {
            options.mouseLook = false;
        }
        else if (name == "--hidden")
        {
            options.hidden = true;
        }
        else if (name == "--aircraft-only")
        {
            options.aircraftOnly = true;
        }
        else if (name == "--no-rotors")
        {
            options.rotors = false;
        }
        else if (name == "--drive")
        {
            options.drive = true;
        }
        else if (name == "--no-pips")
        {
            options.pips = false;
        }
        else if (name == "--no-spin")
        {
            options.spin = false;
        }
        else if (name == "--ui")
        {
            options.overlayPanel = true;
        }
        else if (name == "--main-rotor-angle")
        {
            if (!takeFloat(options.mainRotorAngle))
            {
                return 2;
            }
        }
        else if (name == "--tail-rotor-angle")
        {
            if (!takeFloat(options.tailRotorAngle))
            {
                return 2;
            }
        }
        else if (name == "--tail-pitch")
        {
            if (!takeFloat(options.tailPitch))
            {
                return 2;
            }
        }
        else if (name == "--stabilator-angle")
        {
            if (!takeFloat(options.stabilatorAngle))
            {
                return 2;
            }
        }
        else if (name == "--run-for")
        {
            if (!takeFloat(options.runFor))
            {
                return 2;
            }
        }
        else if (name == "--spin-angle")
        {
            if (!takeFloat(options.spinAngleDegrees))
            {
                return 2;
            }
        }
        else if (name == "--opacity")
        {
            if (!takeFloat(options.opacity))
            {
                return 2;
            }
        }
        else if (name == "--roughness")
        {
            if (!takeFloat(options.cubeRoughness))
            {
                return 2;
            }
            options.overrideCubeMaterial = true;
        }
        else if (name == "--metallic")
        {
            if (!takeFloat(options.cubeMetallic))
            {
                return 2;
            }
            options.overrideCubeMaterial = true;
        }
        else if (name == "--sun")
        {
            if (!takeFloat(options.sunRadiance))
            {
                return 2;
            }
            options.hasSunRadiance = true;
        }
        else if (name == "--sun-texture")
        {
            if (!takeString(options.sunTexturePath))
            {
                return 2;
            }
        }
        else if (name == "--camera")
        {
            float values[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
            bool ok = true;
            for (int component = 0; component < 5 && ok; ++component)
            {
                ok = takeFloat(values[component]);
            }
            if (!ok)
            {
                std::cerr << "选项 --camera 需要 5 个数字: x y z yaw pitch\n";
                return 2;
            }
            options.hasCameraPose = true;
            options.cameraPosition = glm::vec3(values[0], values[1], values[2]);
            options.cameraYawDegrees = values[3];
            options.cameraPitchDegrees = values[4];
        }
        else if (name == "--screenshot")
        {
            if (!takeString(options.screenshotPath))
            {
                return 2;
            }
        }
        else
        {
            std::cerr << "未知选项: " << name << "\n用 --help 查看可用选项。\n";
            return 2;
        }
    }

    if (options.showHelp)
    {
        printUsage();
        return 0;
    }
    if (options.showVersion)
    {
        std::cout << "RenderingCube " << RENDERINGCUBE_VERSION << '\n';
        return 0;
    }

    Application application(1400, 900, "RenderingCube | OpenGL 3.3 PBR");
    // 资产目录先按“可执行文件同级 asset/”解析：构建后整个目录可以拷走直接运行。
    application.setAssetDirectory(asset::resolve(argv[0]));
    // 截图模式不需要窗口：隐藏后仍然正常渲染。
    application.setWindowVisible(!(options.hidden || !options.screenshotPath.empty()));
    application.setAircraftOnly(options.aircraftOnly);

    if (!application.initialize())
    {
        std::cerr << "initialization failed, exiting\n";
        return 1;
    }

    application.setLightDirection(glm::vec3(-0.55f, -1.0f, -0.42f));
    application.setReflectionStrength(options.reflectionStrength);
    application.setObjectOpacity(options.opacity);
    application.enableShadows(options.shadows);
    application.setMouseLookEnabled(options.mouseLook);
    application.setPipsEnabled(options.pips);
    application.setHelicopterRotorsEnabled(options.rotors);
    application.setOverlayPanelVisible(options.overlayPanel);
    application.setHelicopterPose(options.mainRotorAngle, options.tailRotorAngle, options.tailPitch,
                                  options.stabilatorAngle);

    // 自转：命令行只注入初始配置，运行期由 Application 自己推进（空格切换）。
    application.setCubeSpinEnabled(options.spin);
    if (options.spinAngleDegrees >= 0.0f)
    {
        application.setCubeSpinAngle(glm::radians(options.spinAngleDegrees));
    }

    // 太阳：只调独立图层的接口，不碰天空/材质/渲染器。
    if (options.hasSunRadiance)
    {
        SunStyle sunStyle = application.sunStyle();
        sunStyle.radiance = options.sunRadiance;
        application.setSunStyle(sunStyle);
    }
    if (!options.sunTexturePath.empty())
    {
        application.setSunTexture(options.sunTexturePath);
    }

    if (options.hasCameraPose)
    {
        application.setCameraPose(options.cameraPosition, options.cameraYawDegrees, options.cameraPitchDegrees);
    }

    if (options.overrideCubeMaterial)
    {
        // 调试用：保留每面颜色，只把 roughness / metallic 统一覆盖。
        for (int face = 0; face < Application::kCubeFaceCount; ++face)
        {
            PBRMaterial cubeMaterial = application.cubeFaceMaterial(face);
            cubeMaterial.roughness = options.cubeRoughness;
            cubeMaterial.metallic = options.cubeMetallic;
            application.setCubeFaceMaterial(face, cubeMaterial);
        }
    }

    // [启动入口] 先应用相机位置，再尝试驾驶，确保 --camera 与 G 使用相同的距离检查。
    if (options.drive)
    {
        application.setDriving(true);
    }
    if (options.runFor > 0.0f)
    {
        application.run(options.runFor);
    }
    if (!options.screenshotPath.empty())
    {
        return application.captureFrame(options.screenshotPath, 1400, 900) ? 0 : 1;
    }

    if (!(options.runFor > 0.0f))
    {
        application.run();
    }
    return 0;
}

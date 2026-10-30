#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "Application.h"

namespace
{
struct Options
{
    float opacity = 1.0f;
    bool shadows = true;
    float reflectionStrength = 0.55f;
    bool mouseLook = true;
    bool hidden = false;
    bool pips = true;
    bool spin = true;
    float spinAngleDegrees = -1.0f;   // 负数表示"用 Application 的默认角度"
    std::string screenshotPath;
    bool overrideCubeMaterial = false;
    float cubeRoughness = 0.28f;
    float cubeMetallic = 0.0f;
};

bool readFloatArgument(int argc, char** argv, int& index, float& target)
{
    if (index + 1 >= argc)
    {
        return false;
    }
    target = static_cast<float>(std::atof(argv[index + 1]));
    ++index;
    return true;
}
}

int main(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--transparent") == 0)
        {
            options.opacity = 0.62f;
        }
        else if (std::strcmp(argv[i], "--no-shadow") == 0)
        {
            options.shadows = false;
        }
        else if (std::strcmp(argv[i], "--no-reflection") == 0)
        {
            options.reflectionStrength = 0.0f;
        }
        else if (std::strcmp(argv[i], "--no-mouse") == 0)
        {
            options.mouseLook = false;
        }
        else if (std::strcmp(argv[i], "--hidden") == 0)
        {
            options.hidden = true;
        }
        else if (std::strcmp(argv[i], "--no-pips") == 0)
        {
            options.pips = false;
        }
        else if (std::strcmp(argv[i], "--no-spin") == 0)
        {
            options.spin = false;
        }
        else if (std::strcmp(argv[i], "--spin-angle") == 0)
        {
            readFloatArgument(argc, argv, i, options.spinAngleDegrees);
        }
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
        {
            options.screenshotPath = argv[i + 1];
            ++i;
        }
        else if (std::strcmp(argv[i], "--roughness") == 0)
        {
            if (readFloatArgument(argc, argv, i, options.cubeRoughness))
            {
                options.overrideCubeMaterial = true;
            }
        }
        else if (std::strcmp(argv[i], "--metallic") == 0)
        {
            if (readFloatArgument(argc, argv, i, options.cubeMetallic))
            {
                options.overrideCubeMaterial = true;
            }
        }
        else if (std::strcmp(argv[i], "--opacity") == 0)
        {
            readFloatArgument(argc, argv, i, options.opacity);
        }
    }

    Application application(1400, 900, "RenderingCube | OpenGL 3.3 PBR");
    // 截图模式不需要窗口：隐藏后仍然正常渲染。
    application.setWindowVisible(!(options.hidden || !options.screenshotPath.empty()));

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

    // 自转：命令行只注入初始配置，运行期由 Application 自己推进（空格切换）。
    application.setCubeSpinEnabled(options.spin);
    if (options.spinAngleDegrees >= 0.0f)
    {
        application.setCubeSpinAngle(glm::radians(options.spinAngleDegrees));
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

    if (!options.screenshotPath.empty())
    {
        return application.captureFrame(options.screenshotPath, 1400, 900) ? 0 : 1;
    }

    application.run();
    return 0;
}

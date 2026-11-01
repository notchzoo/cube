#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

class Shader;

/// 监视一组 Shader 的源文件（含 #include 依赖）并热重载。
///
/// 设计要点：
///   * 只有链接成功 Shader 才替换句柄，所以着色器写错时画面保持不变，只在控制台留下驱动日志；
///   * 检查按时间节流（默认 0.4 秒一次 stat），不进入每帧热路径；
///   * 依赖文件列表由 Shader 在加载时记录，因此改 common/sky.glsl 会同时触发两个程序重载。
class ShaderWatcher
{
public:
    void add(Shader& shader);
    void clear();

    /// nowSeconds 用单调时钟（glfwGetTime 即可）。返回本次成功重载的程序数量。
    int poll(double nowSeconds);

    /// 最近一次触发重载的文件，供日志或调试面板显示。
    const std::string& lastChangedFile() const { return m_lastChangedFile; }

private:
    struct Watch
    {
        Shader* shader = nullptr;
        std::vector<std::pair<std::string, std::filesystem::file_time_type>> stamps;
        double nextCheck = 0.0;
    };

    static void captureStamps(Watch& watch);
    static bool anyFileChanged(const Watch& watch, std::string& changedPath);

    std::vector<Watch> m_watches;
    std::string m_lastChangedFile;
};

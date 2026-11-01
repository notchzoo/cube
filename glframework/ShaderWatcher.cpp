#include "ShaderWatcher.h"

#include <iostream>

#include "Shader.h"

namespace
{
constexpr double kCheckIntervalSeconds = 0.4;
namespace fs = std::filesystem;

bool readStamp(const std::string& path, fs::file_time_type& stamp)
{
    std::error_code error;
    stamp = fs::last_write_time(path, error);
    return !error;
}
}

void ShaderWatcher::add(Shader& shader)
{
    Watch watch;
    watch.shader = &shader;
    captureStamps(watch);
    m_watches.push_back(std::move(watch));
}

void ShaderWatcher::clear()
{
    m_watches.clear();
    m_lastChangedFile.clear();
}

void ShaderWatcher::captureStamps(Watch& watch)
{
    watch.stamps.clear();
    if (watch.shader == nullptr)
    {
        return;
    }
    for (const std::string& path : watch.shader->dependencyFiles())
    {
        fs::file_time_type stamp;
        if (readStamp(path, stamp))
        {
            watch.stamps.emplace_back(path, stamp);
        }
    }
}

bool ShaderWatcher::anyFileChanged(const Watch& watch, std::string& changedPath)
{
    for (const auto& entry : watch.stamps)
    {
        fs::file_time_type current;
        if (!readStamp(entry.first, current))
        {
            continue;
        }
        if (current > entry.second)
        {
            changedPath = entry.first;
            return true;
        }
    }
    return false;
}

int ShaderWatcher::poll(double nowSeconds)
{
    int reloaded = 0;
    for (Watch& watch : m_watches)
    {
        if (watch.shader == nullptr || nowSeconds < watch.nextCheck)
        {
            continue;
        }
        watch.nextCheck = nowSeconds + kCheckIntervalSeconds;

        std::string changedPath;
        if (!anyFileChanged(watch, changedPath))
        {
            continue;
        }

        if (watch.shader->reload())
        {
            captureStamps(watch);
            m_lastChangedFile = changedPath;
            ++reloaded;
            std::cout << "[Shader] reloaded: " << changedPath << '\n';
        }
        else
        {
            // 保留旧程序；同时记下新的时间戳，避免同一个错误文件每 0.4 秒刷一次日志。
            std::cerr << "[Shader] reload failed, previous program kept: " << changedPath << '\n';
            captureStamps(watch);
        }
    }
    return reloaded;
}

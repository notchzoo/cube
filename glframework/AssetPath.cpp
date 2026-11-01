#include "AssetPath.h"

#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef RENDERINGCUBE_ASSET_DIR
#define RENDERINGCUBE_ASSET_DIR ""
#endif

namespace
{
bool hasShaderDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_directory(path / "shader", error);
}

std::filesystem::path executableDirectory(const char* argv0)
{
#ifdef _WIN32
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0)
    {
        return std::filesystem::path(std::wstring(buffer, length)).parent_path();
    }
#endif
    if (argv0 != nullptr && argv0[0] != '\0')
    {
        return std::filesystem::path(argv0).parent_path();
    }
    return std::filesystem::current_path();
}
}

std::string asset::resolve(const char* argv0)
{
    const std::filesystem::path besideExecutable = executableDirectory(argv0) / "asset";
    if (hasShaderDirectory(besideExecutable))
    {
        return besideExecutable.string();
    }

    const std::filesystem::path buildTimePath = RENDERINGCUBE_ASSET_DIR;
    if (!buildTimePath.empty() && hasShaderDirectory(buildTimePath))
    {
        return buildTimePath.string();
    }

    return besideExecutable.string();
}

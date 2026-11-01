#pragma once

#include <string>

/// 资产目录解析：让可执行文件脱离源码树也能找到 shader。
namespace asset
{
/// 解析顺序：
///   1. 可执行文件同级的 asset/（CMake 在构建后复制，整个目录可搬走）；
///   2. 编译期记录的项目内路径（直接从 IDE / 源码树运行时的兜底）。
/// 两者都不存在时返回第 1 条路径，方便错误信息指向真正期望的位置。
std::string resolve(const char* argv0 = nullptr);
}

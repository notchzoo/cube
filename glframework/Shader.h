#pragma once

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

/// 由顶点/片段源码文件组成的 GLSL 程序。
/// 生命周期：loadFromFiles() 之后可用；reset() 必须早于 GL 上下文销毁。
class Shader
{
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    /// 从磁盘读取并编译链接。失败时打印驱动日志并返回 false，已有程序保持不变。
    /// 源码支持 `#include "相对路径"`：相对当前文件所在目录解析，同一文件只展开一次。
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    /// 重新读取同样两个路径（含其 #include 依赖）并重新链接。
    /// 只有链接成功才替换句柄，因此热重载失败不会把画面变黑。
    bool reload();

    void use() const;

    /// 删除 GL 对象并把句柄清零，可重复调用。
    void reset();

    bool valid() const { return m_program != 0; }
    GLuint id() const { return m_program; }

    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec2(const char* name, const glm::vec2& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setVec4(const char* name, const glm::vec4& value) const;
    void setFloat(const char* name, float value) const;
    void setInt(const char* name, int value) const;

    /// 解析后的 uniform 位置：首次调用查询驱动，之后走缓存。不存在的名字缓存为 -1。
    GLint uniformLocation(const char* name) const;

    /// 重新链接后（或换了程序）必须清空缓存。
    void clearUniformCache() { m_uniformLocations.clear(); }

    /// 该程序读取过的全部源文件（vertex、fragment 及其 #include 依赖），供热重载监视。
    const std::vector<std::string>& dependencyFiles() const { return m_dependencyFiles; }

private:
    static bool readTextFile(const std::string& path, std::string& outSource);
    static std::string directoryOf(const std::string& path);

    /// 递归展开 `#include "..."`；relativeTo 是当前文件所在目录。
    /// included 只记录本阶段的重复包含，m_dependencyFiles 记录全程读过的文件。
    bool expandIncludes(const std::string& source,
                        const std::string& relativeTo,
                        std::string& outSource,
                        std::vector<std::string>& included,
                        int depth);

    static GLuint compileStage(GLenum stage, const std::string& source, const std::string& path);
    static GLuint buildProgram(const std::string& vertexSource,
                               const std::string& vertexPath,
                               const std::string& fragmentSource,
                               const std::string& fragmentPath);

    struct UniformLocationEntry
    {
        std::string name;
        GLint location;
    };

    GLuint m_program = 0;
    std::string m_vertexPath;
    std::string m_fragmentPath;

    std::vector<std::string> m_dependencyFiles;

    /// 名字 -> 位置。名字通常是指向字符串字面量的稳定指针，先比指针再比内容，避免每次构造 std::string。
    mutable std::vector<UniformLocationEntry> m_uniformLocations;
};

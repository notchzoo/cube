#include "Shader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace
{
/// #include 的嵌套上限，防止意外自包含把进程拖死。
constexpr int kMaxIncludeDepth = 8;

bool isAbsolutePath(const std::string& path)
{
    return path.size() > 1 && path[1] == ':';
}
}

Shader::~Shader()
{
    reset();
}

std::string Shader::directoryOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
    {
        return std::string();
    }
    return path.substr(0, slash);
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath)
{
    std::string vertexRaw;
    std::string fragmentRaw;
    if (!readTextFile(vertexPath, vertexRaw))
    {
        std::cerr << "[Shader] cannot read vertex source: " << vertexPath << '\n';
        return false;
    }
    if (!readTextFile(fragmentPath, fragmentRaw))
    {
        std::cerr << "[Shader] cannot read fragment source: " << fragmentPath << '\n';
        return false;
    }

    m_dependencyFiles.clear();
    m_dependencyFiles.push_back(vertexPath);
    m_dependencyFiles.push_back(fragmentPath);

    // 去重只在单个阶段内生效：同一个公共片段可以同时被 vertex 与 fragment 包含。
    std::vector<std::string> vertexIncluded;
    std::vector<std::string> fragmentIncluded;

    std::string vertexSource;
    std::string fragmentSource;
    if (!expandIncludes(vertexRaw, directoryOf(vertexPath), vertexSource, vertexIncluded, 0))
    {
        return false;
    }
    if (!expandIncludes(fragmentRaw, directoryOf(fragmentPath), fragmentSource, fragmentIncluded, 0))
    {
        return false;
    }

    const GLuint program = buildProgram(vertexSource, vertexPath, fragmentSource, fragmentPath);
    if (program == 0)
    {
        return false;
    }

    // 只有新程序链接成功才丢弃旧程序；失败时旧程序保持可用（热重载不会黑屏）。
    reset();
    m_program = program;
    m_vertexPath = vertexPath;
    m_fragmentPath = fragmentPath;
    clearUniformCache();
    return true;
}

bool Shader::reload()
{
    if (m_vertexPath.empty() || m_fragmentPath.empty())
    {
        return false;
    }
    return loadFromFiles(m_vertexPath, m_fragmentPath);
}

bool Shader::expandIncludes(const std::string& source,
                            const std::string& relativeTo,
                            std::string& outSource,
                            std::vector<std::string>& included,
                            int depth)
{
    if (depth > kMaxIncludeDepth)
    {
        std::cerr << "[Shader] #include nesting deeper than " << kMaxIncludeDepth << " levels\n";
        return false;
    }

    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line))
    {
        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line.compare(start, 8, "#include") != 0)
        {
            outSource += line;
            outSource += '\n';
            continue;
        }

        const size_t quoteBegin = line.find('"', start);
        const size_t quoteEnd = (quoteBegin == std::string::npos) ? std::string::npos : line.find('"', quoteBegin + 1);
        if (quoteBegin == std::string::npos || quoteEnd == std::string::npos)
        {
            // 不是 `#include "文件"` 形态（例如系统的尖括号形式），原样交给 GLSL 编译器。
            outSource += line;
            outSource += '\n';
            continue;
        }

        const std::string includeName = line.substr(quoteBegin + 1, quoteEnd - quoteBegin - 1);
        const std::string includePath = isAbsolutePath(includeName) ? includeName : relativeTo + "/" + includeName;

        if (std::find(included.begin(), included.end(), includePath) == included.end())
        {
            std::string includeSource;
            if (!readTextFile(includePath, includeSource))
            {
                std::cerr << "[Shader] cannot read #include \"" << includeName << "\" resolved to " << includePath
                          << '\n';
                return false;
            }
            included.push_back(includePath);
            if (std::find(m_dependencyFiles.begin(), m_dependencyFiles.end(), includePath) == m_dependencyFiles.end())
            {
                m_dependencyFiles.push_back(includePath);
            }
            if (!expandIncludes(includeSource, directoryOf(includePath), outSource, included, depth + 1))
            {
                return false;
            }
            if (!outSource.empty() && outSource.back() != '\n')
            {
                outSource += '\n';
            }
        }

        // 保留行号：被展开的那一行替换成空行。
        outSource += '\n';
    }
    return true;
}

GLuint Shader::buildProgram(const std::string& vertexSource,
                            const std::string& vertexPath,
                            const std::string& fragmentSource,
                            const std::string& fragmentPath)
{
    const GLuint vertexStage = compileStage(GL_VERTEX_SHADER, vertexSource, vertexPath);
    if (vertexStage == 0)
    {
        return 0;
    }

    const GLuint fragmentStage = compileStage(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (fragmentStage == 0)
    {
        glDeleteShader(vertexStage);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexStage);
    glAttachShader(program, fragmentStage);
    glLinkProgram(program);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 1 ? logLength : 1), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::cerr << "[Shader] link failed (" << vertexPath << " + " << fragmentPath << "):\n" << log.data() << '\n';
        glDeleteProgram(program);
        glDeleteShader(vertexStage);
        glDeleteShader(fragmentStage);
        return 0;
    }

    // 链接完成后 stage 不再需要，可执行代码已归程序所有。
    glDetachShader(program, vertexStage);
    glDetachShader(program, fragmentStage);
    glDeleteShader(vertexStage);
    glDeleteShader(fragmentStage);
    return program;
}

void Shader::use() const
{
    glUseProgram(m_program);
}

void Shader::reset()
{
    if (m_program != 0)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    clearUniformCache();
}

GLint Shader::uniformLocation(const char* name) const
{
    if (m_program == 0 || name == nullptr)
    {
        return -1;
    }

    for (const UniformLocationEntry& entry : m_uniformLocations)
    {
        if (entry.name == name)
        {
            return entry.location;
        }
    }

    const GLint location = glGetUniformLocation(m_program, name);
#ifndef NDEBUG
    if (location < 0)
    {
        // 拼错的 uniform 名字过去是静默无效的；Debug 下一律报出来，且只报一次（结果已缓存）。
        std::cerr << "[Shader] uniform '" << name << "' not found in program " << m_program << '\n';
    }
#endif
    m_uniformLocations.push_back({ std::string(name), location });
    return location;
}

void Shader::setMat4(const char* name, const glm::mat4& value) const
{
    const GLint location = uniformLocation(name);
    if (location >= 0)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }
}

void Shader::setVec3(const char* name, const glm::vec3& value) const
{
    const GLint location = uniformLocation(name);
    if (location >= 0)
    {
        glUniform3fv(location, 1, glm::value_ptr(value));
    }
}

void Shader::setVec2(const char* name, const glm::vec2& value) const
{
    const GLint location = uniformLocation(name);
    if (location >= 0)
    {
        glUniform2fv(location, 1, glm::value_ptr(value));
    }
}
void Shader::setVec4(const char* name, const glm::vec4& value) const
{
    const GLint location = uniformLocation(name);
    if (location >= 0)
    {
        glUniform4fv(location, 1, glm::value_ptr(value));
    }
}

void Shader::setFloat(const char* name, float value) const
{
    const GLint location = uniformLocation(name);
    if (location >= 0)
    {
        glUniform1f(location, value);
    }
}

void Shader::setInt(const char* name, int value) const
{
    const GLint location = uniformLocation(name);
    if (location >= 0)
    {
        glUniform1i(location, value);
    }
}

bool Shader::readTextFile(const std::string& path, std::string& outSource)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
    {
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    outSource = buffer.str();
    return !outSource.empty();
}

GLuint Shader::compileStage(GLenum stage, const std::string& source, const std::string& path)
{
    const GLuint shader = glCreateShader(stage);
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    GLint compileStatus = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 1 ? logLength : 1), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::cerr << "[Shader] compile failed (" << path << "):\n" << log.data() << '\n';
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

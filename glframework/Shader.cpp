#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

Shader::~Shader()
{
    reset();
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath)
{
    reset();

    std::string vertexSource;
    std::string fragmentSource;
    if (!readTextFile(vertexPath, vertexSource))
    {
        std::cerr << "[Shader] cannot read vertex source: " << vertexPath << '\n';
        return false;
    }
    if (!readTextFile(fragmentPath, fragmentSource))
    {
        std::cerr << "[Shader] cannot read fragment source: " << fragmentPath << '\n';
        return false;
    }

    const GLuint vertexStage = compileStage(GL_VERTEX_SHADER, vertexSource, vertexPath);
    if (vertexStage == 0)
    {
        return false;
    }

    const GLuint fragmentStage = compileStage(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (fragmentStage == 0)
    {
        glDeleteShader(vertexStage);
        return false;
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
        std::cerr << "[Shader] link failed (" << vertexPath << " + " << fragmentPath << "):\n"
                  << log.data() << '\n';
        glDeleteProgram(program);
        glDeleteShader(vertexStage);
        glDeleteShader(fragmentStage);
        return false;
    }

    // 链接完成后 stage 不再需要，可执行代码已归程序所有。
    glDetachShader(program, vertexStage);
    glDetachShader(program, fragmentStage);
    glDeleteShader(vertexStage);
    glDeleteShader(fragmentStage);

    m_program = program;
    return true;
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
}

void Shader::setMat4(const char* name, const glm::mat4& value) const
{
    glUniformMatrix4fv(glGetUniformLocation(m_program, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const char* name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(value));
}

void Shader::setFloat(const char* name, float value) const
{
    glUniform1f(glGetUniformLocation(m_program, name), value);
}

void Shader::setInt(const char* name, int value) const
{
    glUniform1i(glGetUniformLocation(m_program, name), value);
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
        std::cerr << "[Shader] compile failed (" << path << "):\n"
                  << log.data() << '\n';
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}
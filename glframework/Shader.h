#pragma once

#include <string>

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

    /// 从磁盘读取并编译链接。失败时打印驱动日志并返回 false，对象保持无效状态。
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    void use() const;

    /// 删除 GL 对象并把句柄清零，可重复调用。
    void reset();

    bool valid() const { return m_program != 0; }
    GLuint id() const { return m_program; }

    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setFloat(const char* name, float value) const;
    void setInt(const char* name, int value) const;

private:
    static bool readTextFile(const std::string& path, std::string& outSource);
    static GLuint compileStage(GLenum stage, const std::string& source, const std::string& path);

    GLuint m_program = 0;
};
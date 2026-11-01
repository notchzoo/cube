#pragma once

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

/// 一个索引三角网格的 GPU 资源集合：1 个 VAO、3 个顶点属性 VBO、1 个 EBO。
/// Application 不持有任何裸 GL 缓冲区句柄，几何上传与绘制都经过这里。
class Mesh
{
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    /// 上传位置/颜色/法线三组顶点属性与索引。三个数组长度必须一致。
    /// 属性位置固定为 0=position、1=color、2=normal。
    void upload(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& colors,
                const std::vector<glm::vec3>& normals, const std::vector<unsigned int>& indices);

    void draw() const;

    /// 只绘制索引区间 [firstIndex, firstIndex + indexCount)。
    /// 用于一个 Mesh 内部按材质分组绘制（例如立方体每个面一套材质）。
    void drawRange(int firstIndex, int indexCount) const;

    /// 删除全部 GL 对象并把句柄清零，可重复调用。
    void reset();

    bool valid() const { return m_vao != 0; }
    GLsizei indexCount() const { return m_indexCount; }

private:
    GLuint m_vao = 0;
    GLuint m_positionVbo = 0;
    GLuint m_colorVbo = 0;
    GLuint m_normalVbo = 0;
    GLuint m_ebo = 0;
    GLsizei m_indexCount = 0;
};
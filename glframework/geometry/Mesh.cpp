#include "Mesh.h"

#include <cstddef>
#include <cstdint>
#include <iostream>

Mesh::~Mesh()
{
    reset();
}

void Mesh::upload(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& colors,
                  const std::vector<glm::vec3>& normals, const std::vector<unsigned int>& indices)
{
    if (positions.size() != colors.size() || positions.size() != normals.size() || positions.empty())
    {
        std::cerr << "[Mesh] attribute arrays mismatch or empty, upload skipped\n";
        return;
    }
    if (indices.empty())
    {
        std::cerr << "[Mesh] empty index array, upload skipped\n";
        return;
    }

    reset();

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    const auto uploadAttribute = [](GLuint& vbo, GLuint location, const std::vector<glm::vec3>& data)
    {
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(glm::vec3)), data.data(),
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    };

    uploadAttribute(m_positionVbo, 0, positions);
    uploadAttribute(m_colorVbo, 1, colors);
    uploadAttribute(m_normalVbo, 2, normals);

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    // 解绑 VAO 前不要解绑 EBO：ELEMENT_ARRAY_BUFFER 的绑定属于 VAO 状态。
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_indexCount = static_cast<GLsizei>(indices.size());
}

void Mesh::draw() const
{
    if (m_vao == 0 || m_indexCount == 0)
    {
        return;
    }

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::drawRange(int firstIndex, int indexCount) const
{
    if (m_vao == 0 || m_indexCount == 0 || indexCount <= 0 || firstIndex < 0 || firstIndex + indexCount > m_indexCount)
    {
        return;
    }

    glBindVertexArray(m_vao);
    // 索引类型是 unsigned int，偏移量要换算成字节。
    const auto* offset = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(static_cast<unsigned int>(firstIndex)) * sizeof(unsigned int));
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, offset);
    glBindVertexArray(0);
}

void Mesh::reset()
{
    if (m_ebo != 0)
    {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
    if (m_positionVbo != 0)
    {
        glDeleteBuffers(1, &m_positionVbo);
        m_positionVbo = 0;
    }
    if (m_colorVbo != 0)
    {
        glDeleteBuffers(1, &m_colorVbo);
        m_colorVbo = 0;
    }
    if (m_normalVbo != 0)
    {
        glDeleteBuffers(1, &m_normalVbo);
        m_normalVbo = 0;
    }
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_indexCount = 0;
}
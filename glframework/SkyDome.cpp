#include "SkyDome.h"

#include <cmath>
#include <vector>

namespace
{
constexpr int kStacks = 32;
constexpr int kSectors = 48;
constexpr float kPi = 3.14159265358979323846f;
}

SkyDome::~SkyDome()
{
    reset();
}

bool SkyDome::initialize(const std::string& assetDirectory)
{
    const std::string vertexPath = assetDirectory + "/shader/skyDome.vert";
    const std::string fragmentPath = assetDirectory + "/shader/skyDome.frag";
    if (!m_shader.loadFromFiles(vertexPath, fragmentPath))
    {
        return false;
    }

    createSphere(1.0f);
    return m_sphere.valid();
}

void SkyDome::reset()
{
    m_sphere.reset();
    m_shader.reset();
}

void SkyDome::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const
{
    if (!m_shader.valid() || !m_sphere.valid())
    {
        return;
    }

    // 去掉 View 的平移分量：天空只跟相机朝向走。
    const glm::mat4 rotationOnlyView{ glm::mat3(viewMatrix) };

    m_shader.use();
    m_shader.setMat4("uView", rotationOnlyView);
    m_shader.setMat4("uProjection", projectionMatrix);

    // 顶点着色器把深度压到远平面，因此这里关闭深度写入；深度测试仍然有效。
    glDepthMask(GL_FALSE);
    m_sphere.draw();
    glDepthMask(GL_TRUE);
}

void SkyDome::createSphere(float radius)
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;

    positions.reserve(static_cast<size_t>((kStacks + 1) * (kSectors + 1)));
    colors.reserve(positions.capacity());
    normals.reserve(positions.capacity());

    for (int stack = 0; stack <= kStacks; ++stack)
    {
        const float v = static_cast<float>(stack) / static_cast<float>(kStacks);
        const float phi = v * kPi;
        for (int sector = 0; sector <= kSectors; ++sector)
        {
            const float u = static_cast<float>(sector) / static_cast<float>(kSectors);
            const float theta = u * 2.0f * kPi;

            glm::vec3 direction(std::sin(phi) * std::cos(theta),
                                std::cos(phi),
                                std::sin(phi) * std::sin(theta));
            positions.push_back(direction * radius);
            normals.push_back(direction);
            colors.push_back(glm::vec3(1.0f));
        }
    }

    for (int stack = 0; stack < kStacks; ++stack)
    {
        for (int sector = 0; sector < kSectors; ++sector)
        {
            const unsigned int a = static_cast<unsigned int>(stack * (kSectors + 1) + sector);
            const unsigned int b = a + static_cast<unsigned int>(kSectors + 1);

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(a + 1);

            indices.push_back(a + 1);
            indices.push_back(b);
            indices.push_back(b + 1);
        }
    }

    m_sphere.upload(positions, colors, normals, indices);
}
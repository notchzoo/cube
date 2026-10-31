#include "SunSprite.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace
{
// 太阳放在"无穷远"：这个距离只用来构造 billboard 四边形，不参与任何光照计算。
constexpr float kSunDistance = 100.0f;

unsigned int readU32(const std::vector<unsigned char>& data, size_t offset)
{
    return static_cast<unsigned int>(data[offset]) |
           (static_cast<unsigned int>(data[offset + 1]) << 8) |
           (static_cast<unsigned int>(data[offset + 2]) << 16) |
           (static_cast<unsigned int>(data[offset + 3]) << 24);
}

int readS32(const std::vector<unsigned char>& data, size_t offset)
{
    return static_cast<int>(readU32(data, offset));
}

unsigned int readU16(const std::vector<unsigned char>& data, size_t offset)
{
    return static_cast<unsigned int>(data[offset]) |
           (static_cast<unsigned int>(data[offset + 1]) << 8);
}

/// 最小 BMP 读取：只支持未压缩的 24/32 位 BMP，输出 RGBA8。
bool loadBmpRgba(const std::string& path, std::vector<unsigned char>& rgba, int& width, int& height)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
    {
        return false;
    }

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),
                                     std::istreambuf_iterator<char>());
    if (bytes.size() < 54 || bytes[0] != 'B' || bytes[1] != 'M')
    {
        return false;
    }

    const unsigned int dataOffset = readU32(bytes, 10);
    const unsigned int headerSize = readU32(bytes, 14);
    if (headerSize < 40 || bytes.size() < dataOffset)
    {
        return false;
    }

    const int rawWidth = readS32(bytes, 18);
    const int rawHeight = readS32(bytes, 22);
    const unsigned int bitsPerPixel = readU16(bytes, 28);
    const unsigned int compression = readU32(bytes, 30);
    if ((bitsPerPixel != 24 && bitsPerPixel != 32) || compression != 0)
    {
        return false;
    }
    if (rawWidth <= 0 || rawHeight == 0)
    {
        return false;
    }

    const int w = rawWidth;
    const int h = std::abs(rawHeight);
    const bool bottomUp = rawHeight > 0;
    const int bytesPerPixel = static_cast<int>(bitsPerPixel / 8);
    const size_t rowBytes = static_cast<size_t>((w * bytesPerPixel + 3) & ~3);

    if (bytes.size() < dataOffset + rowBytes * static_cast<size_t>(h))
    {
        return false;
    }

    rgba.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 255);
    for (int y = 0; y < h; ++y)
    {
        const int sourceRow = bottomUp ? (h - 1 - y) : y;
        const unsigned char* source = bytes.data() + dataOffset + rowBytes * static_cast<size_t>(sourceRow);
        unsigned char* target = rgba.data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
        for (int x = 0; x < w; ++x)
        {
            target[x * 4 + 0] = source[x * bytesPerPixel + 2];   // B -> R
            target[x * 4 + 1] = source[x * bytesPerPixel + 1];   // G
            target[x * 4 + 2] = source[x * bytesPerPixel + 0];   // R -> B
            target[x * 4 + 3] = (bytesPerPixel == 4) ? source[x * bytesPerPixel + 3] : 255;
        }
    }

    width = w;
    height = h;
    return true;
}
}

SunSprite::~SunSprite()
{
    reset();
}

bool SunSprite::initialize(const std::string& assetDirectory)
{
    if (!m_shader.loadFromFiles(assetDirectory + "/shader/sun.vert",
                                assetDirectory + "/shader/sun.frag"))
    {
        return false;
    }

    createQuad();
    return m_vao != 0;
}

void SunSprite::reset()
{
    if (m_texture != 0)
    {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_shader.reset();
}

void SunSprite::createQuad()
{
    // 两个三角形覆盖 [-1,1]^2，本地坐标直接交给片元着色器当作径向参数。
    const float corners[12] = {
        -1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,
        -1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f,
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool SunSprite::setTextureFromBmp(const std::string& path)
{
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    if (!loadBmpRgba(path, pixels, width, height))
    {
        std::cerr << "[SunSprite] cannot load BMP: " << path << '\n';
        return false;
    }

    uploadTexture(pixels.data(), width, height);
    std::cout << "[SunSprite] texture loaded: " << path << " (" << width << "x" << height << ")\n";
    return true;
}

void SunSprite::uploadTexture(const unsigned char* rgba, int width, int height)
{
    if (m_texture == 0)
    {
        glGenTextures(1, &m_texture);
    }

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SunSprite::clearTexture()
{
    if (m_texture != 0)
    {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

void SunSprite::render(const glm::vec3& cameraPosition,
                       const glm::mat4& viewMatrix,
                       const glm::mat4& projectionMatrix,
                       const glm::vec3& sunDirection)
{
    if (!m_shader.valid() || m_vao == 0)
    {
        return;
    }

    const float directionLength = glm::length(sunDirection);
    if (directionLength < 1e-5f)
    {
        return;
    }
    const glm::vec3 sunDir = sunDirection / directionLength;

    // billboard 基向量。太阳"无穷远"，这里放在固定距离只为构造四边形。
    glm::vec3 referenceUp(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(sunDir, referenceUp)) > 0.999f)
    {
        referenceUp = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    const glm::vec3 right = glm::normalize(glm::cross(sunDir, referenceUp));
    const glm::vec3 up = glm::cross(right, sunDir);

    const float glowDegrees = std::max(m_style.glowDegrees, 0.05f);
    const float halfSize = kSunDistance * std::tan(glm::radians(glowDegrees));

    m_shader.use();
    m_shader.setMat4("uView", viewMatrix);
    m_shader.setMat4("uProjection", projectionMatrix);
    m_shader.setVec3("uSunCenter", cameraPosition + sunDir * kSunDistance);
    m_shader.setVec3("uSunRight", right);
    m_shader.setVec3("uSunUp", up);
    m_shader.setFloat("uHalfSize", halfSize);
    m_shader.setVec3("uTint", m_style.tint);
    m_shader.setFloat("uRadiance", std::max(m_style.radiance, 0.0f));
    m_shader.setFloat("uCoreRatio",
                      std::clamp(m_style.coreDegrees / glowDegrees, 0.01f, 1.0f));
    m_shader.setInt("uRayCount", std::max(m_style.rayCount, 0));
    m_shader.setFloat("uRaySharpness", std::max(m_style.raySharpness, 1.0f));
    m_shader.setFloat("uRayStrength", std::max(m_style.rayStrength, 0.0f));
    m_shader.setInt("uUseTexture", m_texture != 0 ? 1 : 0);
    if (m_texture != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        m_shader.setInt("uSunTexture", 0);
    }

    // 太阳在无穷远：不做深度测试、不写深度。后画的地面/骰子会自然盖住它，
    // 所以地平线以下的部分不需要额外裁剪。
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
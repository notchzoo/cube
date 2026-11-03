#include "Texture2D.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
std::uint16_t read16(const std::vector<unsigned char>& data, std::size_t offset)
{
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t read32(const std::vector<unsigned char>& data, std::size_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}
} // namespace

Texture2D::~Texture2D()
{
    reset();
}

void Texture2D::reset()
{
    if (m_id != 0)
    {
        glDeleteTextures(1, &m_id);
        m_id = 0;
        m_width = 0;
        m_height = 0;
    }
}

bool Texture2D::loadBmp(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "[Texture2D] cannot open " << path << '\n';
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 54)
    {
        std::cerr << "[Texture2D] BMP too small: " << path << '\n';
        return false;
    }

    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file)
        return false;

    if (data[0] != 'B' || data[1] != 'M')
    {
        std::cerr << "[Texture2D] not a BMP: " << path << '\n';
        return false;
    }

    const std::uint32_t pixelOffset = read32(data, 10);
    const std::uint32_t headerSize = read32(data, 14);
    if (headerSize < 40)
    {
        std::cerr << "[Texture2D] unsupported BMP header: " << path << '\n';
        return false;
    }

    const std::int32_t width = static_cast<std::int32_t>(read32(data, 18));
    const std::int32_t rawHeight = static_cast<std::int32_t>(read32(data, 22));
    const std::uint16_t planes = read16(data, 26);
    const std::uint16_t bpp = read16(data, 28);
    const std::uint32_t compression = read32(data, 30);
    if (width <= 0 || rawHeight == 0 || planes != 1 || (bpp != 24 && bpp != 32) || compression != 0)
    {
        std::cerr << "[Texture2D] unsupported BMP format: " << path << '\n';
        return false;
    }

    const int height = rawHeight < 0 ? -rawHeight : rawHeight;
    const bool topDown = rawHeight < 0;
    const int bytesPerPixel = bpp / 8;
    const int rowStride = ((width * bpp + 31) / 32) * 4;
    if (pixelOffset + static_cast<std::size_t>(rowStride) * static_cast<std::size_t>(height) > data.size())
    {
        std::cerr << "[Texture2D] BMP pixel data truncated: " << path << '\n';
        return false;
    }

    std::vector<unsigned char> rgba(static_cast<std::size_t>(width) * height * 4);
    for (int row = 0; row < height; ++row)
    {
        // GL 纹理第 0 行对应图片底部；BMP 正高度本身按 bottom-up 存储。
        const int sourceRow = topDown ? (height - 1 - row) : row;
        const unsigned char* source = data.data() + pixelOffset + static_cast<std::size_t>(sourceRow) * rowStride;
        unsigned char* destination = rgba.data() + static_cast<std::size_t>(row) * width * 4;
        for (int x = 0; x < width; ++x)
        {
            const unsigned char* pixel = source + x * bytesPerPixel;
            destination[x * 4 + 0] = pixel[2];
            destination[x * 4 + 1] = pixel[1];
            destination[x * 4 + 2] = pixel[0];
            destination[x * 4 + 3] = bpp == 32 ? pixel[3] : 255;
        }
    }

    if (m_id == 0)
        glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_width = width;
    m_height = height;
    std::cout << "[Texture2D] loaded " << path << " (" << width << "x" << height << ")\n";
    return true;
}

void Texture2D::bind(int unit) const
{
    if (m_id == 0)
        return;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

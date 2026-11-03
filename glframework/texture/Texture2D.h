#pragma once

#include <string>

#include <glad/glad.h>

/// 简单的 RGBA 纹理：BMP 加载 + 绑定。模型 livery 使用顶部/腹部两张贴图。
class Texture2D
{
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    bool loadBmp(const std::string& path);
    void bind(int unit) const;
    void reset();

    bool valid() const { return m_id != 0; }
    GLuint id() const { return m_id; }
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
};

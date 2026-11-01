#include "Overlay.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
constexpr int kAtlasWidth = 1024;
constexpr int kAtlasHeight = 1024;
constexpr float kRowPadding = 8.0f;
constexpr float kLabelWidth = 86.0f;
constexpr float kValueWidth = 64.0f;

const glm::vec4 kLabelColor{0.78f, 0.82f, 0.88f, 1.0f};
const glm::vec4 kValueColor{0.95f, 0.97f, 1.0f, 1.0f};
const glm::vec4 kSectionColor{0.55f, 0.66f, 0.85f, 1.0f};

/// 一次性保存/恢复覆盖层会碰到的 GL 状态，避免手工 restore 漏项污染后续帧。
struct GlStateBlock
{
    GLint program = 0;
    GLint vertexArray = 0;
    GLint activeTexture = 0;
    GLint texture = 0;
    GLint blendSourceRgb = 0;
    GLint blendDestinationRgb = 0;
    GLint blendSourceAlpha = 0;
    GLint blendDestinationAlpha = 0;
    GLint blendEquationRgb = 0;
    GLint blendEquationAlpha = 0;
    GLboolean depthTest = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLboolean cullFace = GL_FALSE;
    GLboolean depthMask = GL_TRUE;
    GLboolean scissor = GL_FALSE;

    GlStateBlock()
    {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
        depthTest = glIsEnabled(GL_DEPTH_TEST);
        blend = glIsEnabled(GL_BLEND);
        cullFace = glIsEnabled(GL_CULL_FACE);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    }

    ~GlStateBlock()
    {
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vertexArray));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glActiveTexture(static_cast<GLenum>(activeTexture));
        glBlendFuncSeparate(static_cast<GLenum>(blendSourceRgb), static_cast<GLenum>(blendDestinationRgb),
                            static_cast<GLenum>(blendSourceAlpha), static_cast<GLenum>(blendDestinationAlpha));
        glBlendEquationSeparate(static_cast<GLenum>(blendEquationRgb), static_cast<GLenum>(blendEquationAlpha));
        glDepthMask(depthMask);
        if (depthTest == GL_TRUE)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        if (blend == GL_TRUE)
        {
            glEnable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
        }
        if (cullFace == GL_TRUE)
        {
            glEnable(GL_CULL_FACE);
        }
        else
        {
            glDisable(GL_CULL_FACE);
        }
        if (scissor == GL_TRUE)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }
};

/// 把 UTF-8 解码成一个码点，返回消耗的字节数（0 表示非法序列）。
int decodeUtf8(const std::string& text, size_t offset, unsigned int& codepoint)
{
    const unsigned char first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80)
    {
        codepoint = first;
        return 1;
    }
    int extra = 0;
    unsigned int value = 0;
    if ((first & 0xE0) == 0xC0)
    {
        extra = 1;
        value = first & 0x1F;
    }
    else if ((first & 0xF0) == 0xE0)
    {
        extra = 2;
        value = first & 0x0F;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        extra = 3;
        value = first & 0x07;
    }
    else
    {
        return 0;
    }
    if (offset + static_cast<size_t>(extra) >= text.size())
    {
        return 0;
    }
    for (int index = 1; index <= extra; ++index)
    {
        const unsigned char next = static_cast<unsigned char>(text[offset + static_cast<size_t>(index)]);
        if ((next & 0xC0) != 0x80)
        {
            return 0;
        }
        value = (value << 6) | (next & 0x3F);
    }
    codepoint = value;
    return extra + 1;
}

#ifdef _WIN32
/// GDI 只用来把字形画进图集：每个码点一次，之后每帧只做纹理采样。
struct TextSurface
{
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HFONT font = nullptr;
    HGDIOBJ previousBitmap = nullptr;
    HGDIOBJ previousFont = nullptr;
    unsigned char* pixels = nullptr;
    int lineHeight = 0;
    int ascent = 0;

    bool initialize()
    {
        dc = ::CreateCompatibleDC(nullptr);
        if (dc == nullptr)
        {
            return false;
        }

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = kAtlasWidth;
        info.bmiHeader.biHeight = -kAtlasHeight; // 负高度 = 自上而下，和纹理坐标一致
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = ::CreateDIBSection(dc, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (bitmap == nullptr || pixels == nullptr)
        {
            return false;
        }
        previousBitmap = ::SelectObject(dc, bitmap);
        std::memset(pixels, 0, static_cast<size_t>(kAtlasWidth) * kAtlasHeight * 4u);

        // 中文字体优先；缺失时 GDI 会退回系统默认字体，至少不会崩。
        font = ::CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        if (font == nullptr)
        {
            return false;
        }
        previousFont = ::SelectObject(dc, font);
        ::SetTextColor(dc, RGB(255, 255, 255));
        ::SetBkMode(dc, TRANSPARENT);

        TEXTMETRICW metrics{};
        ::GetTextMetricsW(dc, &metrics);
        lineHeight = metrics.tmHeight;
        ascent = metrics.tmAscent;
        return lineHeight > 0;
    }

    ~TextSurface()
    {
        if (dc != nullptr && previousFont != nullptr)
        {
            ::SelectObject(dc, previousFont);
        }
        if (dc != nullptr && previousBitmap != nullptr)
        {
            ::SelectObject(dc, previousBitmap);
        }
        if (font != nullptr)
        {
            ::DeleteObject(font);
        }
        if (bitmap != nullptr)
        {
            ::DeleteObject(bitmap);
        }
        if (dc != nullptr)
        {
            ::DeleteDC(dc);
        }
    }
};
#endif
}

Overlay::~Overlay()
{
    reset();
}

bool Overlay::initialize(const std::string& assetDirectory)
{
    reset();

    if (!m_shader.loadFromFiles(assetDirectory + "/shader/ui/overlay.vert", assetDirectory + "/shader/ui/overlay.frag"))
    {
        std::cerr << "[Overlay] shader load failed\n";
        return false;
    }

    m_atlasWidth = kAtlasWidth;
    m_atlasHeight = kAtlasHeight;
    m_atlasPixels.assign(static_cast<size_t>(m_atlasWidth) * static_cast<size_t>(m_atlasHeight), 0);

    glGenTextures(1, m_atlasTexture.address());
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_atlasWidth, m_atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE,
                 m_atlasPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenVertexArrays(1, m_vertexArray.address());
    glGenBuffers(1, m_vertexBuffer.address());
    glBindVertexArray(m_vertexArray.id());
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.id());

    const GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, local)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, halfSize)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, radius)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, kind)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    initializeTextBackend();
    return true;
}

bool Overlay::initializeTextBackend()
{
#ifdef _WIN32
    auto* surface = new TextSurface();
    if (!surface->initialize())
    {
        std::cerr << "[Overlay] text backend unavailable; UI text will be blank\n";
        delete surface;
        return false;
    }
    m_textSurface = surface;
    m_lineHeight = static_cast<float>(surface->lineHeight);
    return true;
#else
    std::cerr << "[Overlay] text backend is Windows-only; UI text will be blank\n";
    return false;
#endif
}

void Overlay::releaseTextBackend()
{
#ifdef _WIN32
    delete static_cast<TextSurface*>(m_textSurface);
#endif
    m_textSurface = nullptr;
    m_glyphs.clear();
}

void Overlay::reset()
{
    releaseTextBackend();
    m_vertices.clear();
    m_vertexArray.reset();
    m_vertexBuffer.reset();
    m_atlasTexture.reset();
    m_shader.reset();
    m_atlasPixels.clear();
    m_panelOpen = false;
}

void Overlay::beginFrame(int framebufferWidth, int framebufferHeight)
{
    m_width = framebufferWidth;
    m_height = framebufferHeight;
    m_vertices.clear();
    m_panelOpen = false;
}

void Overlay::setMouse(float x, float y, bool down)
{
    m_mouse = glm::vec2(x, y);
    m_mouseDown = down;
}


void Overlay::addQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float radius, float kind,
                      const glm::vec2& uv0, const glm::vec2& uv1)
{
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        return;
    }

    const glm::vec2 center = position + size * 0.5f;
    const glm::vec2 halfSize = size * 0.5f;
    const glm::vec2 uv1Adjusted = uv1;

    const glm::vec2 corners[4] = {position, position + glm::vec2(size.x, 0.0f), position + size,
                                  position + glm::vec2(0.0f, size.y)};
    const glm::vec2 uvs[4] = {uv0, glm::vec2(uv1Adjusted.x, uv0.y), uv1Adjusted, glm::vec2(uv0.x, uv1Adjusted.y)};
    const int indices[6] = {0, 1, 2, 0, 2, 3};

    for (int index : indices)
    {
        Vertex vertex;
        vertex.position = corners[index];
        vertex.local = corners[index] - center;
        vertex.halfSize = halfSize;
        vertex.uv = uvs[index];
        vertex.color = color;
        vertex.radius = radius;
        vertex.kind = kind;
        m_vertices.push_back(vertex);
    }
}

void Overlay::fillRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float radius)
{
    addQuad(position, size, color, radius, 0.0f, glm::vec2(0.0f), glm::vec2(0.0f));
}

const Overlay::Glyph* Overlay::glyph(unsigned int codepoint)
{
    if (codepoint == '\r' || codepoint == '\n')
    {
        return nullptr;
    }
    auto found = m_glyphs.find(codepoint);
    if (found != m_glyphs.end())
    {
        return &found->second;
    }
    rasterizeGlyph(codepoint);
    found = m_glyphs.find(codepoint);
    return (found != m_glyphs.end()) ? &found->second : nullptr;
}

void Overlay::rasterizeGlyph(unsigned int codepoint)
{
#ifdef _WIN32
    auto* surface = static_cast<TextSurface*>(m_textSurface);
    if (surface == nullptr || m_atlasPixels.empty())
    {
        return;
    }
    if (codepoint > 0xFFFF)
    {
        return; // 图集只收录 BMP，够 UI 用
    }

    const wchar_t wide = static_cast<wchar_t>(codepoint);
    SIZE extent{};
    if (::GetTextExtentPoint32W(surface->dc, &wide, 1, &extent) == FALSE)
    {
        return;
    }
    const int advance = std::max(1, static_cast<int>(extent.cx));
    const int cellWidth = advance + 1; // 多留一列，避免相邻字形采样串色
    const int cellHeight = std::max(1, surface->lineHeight);

    if (m_atlasPenX + cellWidth > m_atlasWidth)
    {
        m_atlasPenX = 0;
        m_atlasPenY += m_atlasShelfHeight + 1;
        m_atlasShelfHeight = 0;
    }
    if (m_atlasPenY + cellHeight > m_atlasHeight)
    {
        static bool warned = false;
        if (!warned)
        {
            std::cerr << "[Overlay] glyph atlas is full; further glyphs are dropped\n";
            warned = true;
        }
        return;
    }

    // 清空该单元格，再把字形画进去。
    for (int y = 0; y < cellHeight; ++y)
    {
        std::memset(surface->pixels + ((m_atlasPenY + y) * m_atlasWidth + m_atlasPenX) * 4, 0,
                    static_cast<size_t>(cellWidth) * 4u);
    }
    ::TextOutW(surface->dc, m_atlasPenX, m_atlasPenY, &wide, 1);
    ::GdiFlush();

    for (int y = 0; y < cellHeight; ++y)
    {
        for (int x = 0; x < advance; ++x)
        {
            const int sourceX = m_atlasPenX + x;
            const int sourceY = m_atlasPenY + y;
            const size_t sourceIndex = (static_cast<size_t>(sourceY) * m_atlasWidth + sourceX) * 4u;
            const unsigned char coverage = std::max(
                {surface->pixels[sourceIndex], surface->pixels[sourceIndex + 1], surface->pixels[sourceIndex + 2]});
            m_atlasPixels[static_cast<size_t>(sourceY) * m_atlasWidth + sourceX] = coverage;
        }
    }

    Glyph glyphData;
    glyphData.uv0 = glm::vec2(static_cast<float>(m_atlasPenX) / m_atlasWidth,
                              static_cast<float>(m_atlasPenY) / m_atlasHeight);
    glyphData.uv1 = glm::vec2(static_cast<float>(m_atlasPenX + advance) / m_atlasWidth,
                              static_cast<float>(m_atlasPenY + cellHeight) / m_atlasHeight);
    glyphData.advance = static_cast<float>(advance);
    m_glyphs.emplace(codepoint, glyphData);

    m_atlasPenX += cellWidth;
    m_atlasShelfHeight = std::max(m_atlasShelfHeight, cellHeight);
    m_atlasDirty = true;
#else
    (void)codepoint;
#endif
}

void Overlay::text(const glm::vec2& position, const std::string& utf8, const glm::vec4& color, float scale)
{
    if (m_textSurface == nullptr || utf8.empty())
    {
        return;
    }

    float penX = position.x;
    size_t offset = 0;
    while (offset < utf8.size())
    {
        unsigned int codepoint = 0;
        const int consumed = decodeUtf8(utf8, offset, codepoint);
        if (consumed <= 0)
        {
            ++offset;
            continue;
        }
        offset += static_cast<size_t>(consumed);

        const Glyph* glyphData = glyph(codepoint);
        if (glyphData == nullptr)
        {
            continue;
        }
        addQuad(glm::vec2(penX, position.y), glm::vec2(glyphData->advance * scale, m_lineHeight * scale), color, 0.0f,
                1.0f, glyphData->uv0, glyphData->uv1);
        penX += glyphData->advance * scale;
    }
}

float Overlay::textWidth(const std::string& utf8, float scale)
{
    if (m_textSurface == nullptr || utf8.empty())
    {
        return 0.0f;
    }

    float width = 0.0f;
    size_t offset = 0;
    while (offset < utf8.size())
    {
        unsigned int codepoint = 0;
        const int consumed = decodeUtf8(utf8, offset, codepoint);
        if (consumed <= 0)
        {
            ++offset;
            continue;
        }
        offset += static_cast<size_t>(consumed);
        const Glyph* glyphData = glyph(codepoint);
        if (glyphData != nullptr)
        {
            width += glyphData->advance * scale;
        }
    }
    return width;
}

bool Overlay::mouseInside(const glm::vec2& position, const glm::vec2& size) const
{
    return m_mouse.x >= position.x && m_mouse.x <= position.x + size.x && m_mouse.y >= position.y &&
           m_mouse.y <= position.y + size.y;
}

void Overlay::beginPanel(const glm::vec2& position, float width, float height, const std::string& title)
{
    m_panelOpen = true;
    m_panelRect = glm::vec4(position.x, position.y, width, height);

    fillRect(position + glm::vec2(2.0f, 3.0f), glm::vec2(width, height), glm::vec4(0.0f, 0.0f, 0.0f, 0.35f), 10.0f);
    fillRect(position, glm::vec2(width, height), glm::vec4(0.07f, 0.08f, 0.10f, 0.94f), 10.0f);
    fillRect(position + glm::vec2(14.0f, 10.0f + m_lineHeight + 6.0f), glm::vec2(width - 28.0f, 1.0f),
             glm::vec4(0.30f, 0.34f, 0.42f, 0.9f), 0.0f);

    text(position + glm::vec2(14.0f, 10.0f), title, glm::vec4(0.86f, 0.92f, 1.0f, 1.0f), 1.1f);
    m_cursorY = position.y + 10.0f + m_lineHeight * 1.1f + 12.0f;
    m_contentLeft = position.x + 14.0f;
    m_contentRight = position.x + width - 14.0f;
}

void Overlay::endPanel()
{
    m_cursorY = 0.0f;
}

void Overlay::section(const std::string& title)
{
    const float rowY = m_cursorY;
    m_cursorY += m_rowHeight;
    text(glm::vec2(m_contentLeft, rowY + 4.0f), title, kSectionColor, 0.95f);
    fillRect(glm::vec2(m_contentLeft, rowY + m_rowHeight - 3.0f),
             glm::vec2(m_contentRight - m_contentLeft, 1.0f), glm::vec4(0.30f, 0.34f, 0.42f, 0.55f), 0.0f);
}

void Overlay::valueRow(const std::string& label, const std::string& value)
{
    const float rowY = m_cursorY;
    m_cursorY += m_rowHeight;
    text(glm::vec2(m_contentLeft, rowY + 3.0f), label, kLabelColor, 1.0f);
    const float width = textWidth(value, 1.0f);
    text(glm::vec2(m_contentRight - width, rowY + 3.0f), value, kValueColor, 1.0f);
}

bool Overlay::sliderRow(const std::string& label, float& value, float minimum, float maximum, const char* format)
{
    const float rowY = m_cursorY;
    m_cursorY += m_rowHeight;

    const float trackX = m_contentLeft + kLabelWidth;
    const float trackWidth = std::max(24.0f, m_contentRight - kValueWidth - trackX);
    const float trackY = rowY + m_rowHeight * 0.5f - 3.0f;

    text(glm::vec2(m_contentLeft, rowY + 3.0f), label, kLabelColor, 1.0f);

    const float span = (maximum > minimum) ? (maximum - minimum) : 1.0f;
    float normalized = std::clamp((value - minimum) / span, 0.0f, 1.0f);

    fillRect(glm::vec2(trackX, trackY), glm::vec2(trackWidth, 6.0f), glm::vec4(0.18f, 0.20f, 0.25f, 1.0f), 3.0f);
    fillRect(glm::vec2(trackX, trackY), glm::vec2(trackWidth * normalized, 6.0f),
             glm::vec4(0.20f, 0.62f, 0.95f, 1.0f), 3.0f);
    fillRect(glm::vec2(trackX + trackWidth * normalized - 4.0f, trackY - 3.0f), glm::vec2(8.0f, 12.0f),
             glm::vec4(0.90f, 0.94f, 1.0f, 1.0f), 3.0f);

    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), format, value);
    text(glm::vec2(m_contentRight - textWidth(buffer, 1.0f), rowY + 3.0f), buffer, kValueColor, 1.0f);

    bool changed = false;
    const glm::vec2 hitPosition(trackX - 8.0f, rowY);
    const glm::vec2 hitSize(trackWidth + 16.0f, m_rowHeight);

    if (mouseInside(hitPosition, hitSize) && mousePressed())
    {
        m_activeSlider = &value;
    }
    if (m_activeSlider == &value)
    {
        if (m_mouseDown)
        {
            normalized = std::clamp((m_mouse.x - trackX) / std::max(1.0f, trackWidth), 0.0f, 1.0f);
            const float updated = minimum + normalized * span;
            if (updated != value)
            {
                value = updated;
                changed = true;
            }
        }
        else
        {
            m_activeSlider = nullptr;
        }
    }
    return changed;
}

bool Overlay::checkboxRow(const std::string& label, bool& value)
{
    const float rowY = m_cursorY;
    m_cursorY += m_rowHeight;

    const glm::vec2 boxPosition(m_contentLeft, rowY + (m_rowHeight - 16.0f) * 0.5f);
    fillRect(boxPosition, glm::vec2(16.0f, 16.0f), glm::vec4(0.16f, 0.18f, 0.23f, 1.0f), 4.0f);
    if (value)
    {
        fillRect(boxPosition + glm::vec2(3.0f, 3.0f), glm::vec2(10.0f, 10.0f), glm::vec4(0.20f, 0.62f, 0.95f, 1.0f),
                 2.0f);
    }
    text(glm::vec2(m_contentLeft + 26.0f, rowY + 3.0f), label, kLabelColor, 1.0f);

    const glm::vec2 hitPosition(m_contentLeft, rowY);
    const glm::vec2 hitSize(m_contentRight - m_contentLeft, m_rowHeight);
    if (mouseInside(hitPosition, hitSize) && mousePressed())
    {
        value = !value;
        return true;
    }
    return false;
}

bool Overlay::buttonRow(const std::string& label)
{
    const float rowY = m_cursorY;
    m_cursorY += m_rowHeight;

    const glm::vec2 position(m_contentLeft, rowY + 2.0f);
    const glm::vec2 size(m_contentRight - m_contentLeft, m_rowHeight - 4.0f);
    const bool hovered = mouseInside(position, size);

    fillRect(position, size, hovered ? glm::vec4(0.24f, 0.30f, 0.40f, 1.0f) : glm::vec4(0.16f, 0.19f, 0.25f, 1.0f),
             5.0f);
    const float width = textWidth(label, 1.0f);
    text(glm::vec2(position.x + (size.x - width) * 0.5f, rowY + 3.0f), label, kValueColor, 1.0f);

    return hovered && mousePressed();
}

bool Overlay::panelHovered() const
{
    if (!m_panelOpen || m_panelRect.z <= 0.0f)
    {
        return false;
    }
    return m_mouse.x >= m_panelRect.x && m_mouse.x <= m_panelRect.x + m_panelRect.z && m_mouse.y >= m_panelRect.y &&
           m_mouse.y <= m_panelRect.y + m_panelRect.w;
}

void Overlay::uploadAtlas()
{
    if (!m_atlasDirty || !m_atlasTexture.valid())
    {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture.id());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_atlasWidth, m_atlasHeight, GL_RED, GL_UNSIGNED_BYTE,
                    m_atlasPixels.data());
    m_atlasDirty = false;
}

void Overlay::endFrame()
{
    m_mouseDownPrevious = m_mouseDown;

    if (m_vertices.empty() || !m_shader.valid() || m_width <= 0 || m_height <= 0)
    {
        return;
    }

    uploadAtlas();

    GlStateBlock stateBlock;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.use();
    m_shader.setInt("uAtlas", 0);
    m_shader.setVec2("uViewport", glm::vec2(static_cast<float>(m_width), static_cast<float>(m_height)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture.id());
    glBindVertexArray(m_vertexArray.id());
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.id());
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl::checkError("overlay: draw");
}

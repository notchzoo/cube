#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "GlUtil.h"
#include "Shader.h"

/// 轻量即时模式覆盖层。
///
/// 只解决这个项目真正需要的三件事：屏幕文字、圆角面板、几个调参控件（滑杆 / 开关 / 按钮）。
/// 它不引入第三方 UI 库，也不在每帧用 glGet* 逐个保存十几个 GL 状态：所有图元先累积到
/// 顶点数组，由 endFrame() 在一个显式保存/恢复的状态块里一次性画完。
///
/// 文字栅格化在 Windows 上借用系统字体（GDI 只负责“把字形画进图集”，每帧零 GDI 调用）；
/// 其它平台退化为不显示文字，三维渲染不受影响。
class Overlay
{
public:
    Overlay() = default;
    ~Overlay();

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool initialize(const std::string& assetDirectory);
    void reset();

    /// 每帧开始：清空上一帧的顶点，记录视口尺寸。
    void beginFrame(int framebufferWidth, int framebufferHeight);
    /// 提交并绘制。必须在三维场景之后、交换缓冲之前调用。
    void endFrame();

    /// 行高（像素），供调用方计算面板高度。
    float rowHeight() const { return m_rowHeight; }

    /// 鼠标状态由 Application 每帧喂进来（像素坐标，左上原点）。
    void setMouse(float x, float y, bool down);

    // --- 基础图元 ---
    void fillRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float radius = 0.0f);
    void text(const glm::vec2& position, const std::string& utf8, const glm::vec4& color, float scale = 1.0f);
    float textWidth(const std::string& utf8, float scale = 1.0f);
    float lineHeight() const { return m_lineHeight; }

    // --- 面板与行式控件（返回 true 表示本帧被修改）---
    void beginPanel(const glm::vec2& position, float width, float height, const std::string& title);
    void endPanel();
    void section(const std::string& title);
    void valueRow(const std::string& label, const std::string& value);
    bool sliderRow(const std::string& label, float& value, float minimum, float maximum, const char* format);
    bool checkboxRow(const std::string& label, bool& value);
    bool buttonRow(const std::string& label);

    /// 鼠标是否落在面板内（决定要不要让出光标、屏蔽视角旋转）。
    bool panelHovered() const;
    bool panelOpen() const { return m_panelOpen; }

    /// 供热重载读取底层程序。
    Shader& shader() { return m_shader; }
    const Shader& shader() const { return m_shader; }

private:
    struct Vertex
    {
        glm::vec2 position;
        glm::vec2 local;
        glm::vec2 halfSize;
        glm::vec2 uv;
        glm::vec4 color;
        float radius;
        float kind;
    };

    struct Glyph
    {
        glm::vec2 uv0{0.0f, 0.0f};
        glm::vec2 uv1{0.0f, 0.0f};
        float advance = 0.0f;
    };

    void addQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, float radius, float kind,
                 const glm::vec2& uv0, const glm::vec2& uv1);
    const Glyph* glyph(unsigned int codepoint);

    bool initializeTextBackend();
    void releaseTextBackend();
    void rasterizeGlyph(unsigned int codepoint);
    void uploadAtlas();

    void beginRow();
    bool mouseInside(const glm::vec2& position, const glm::vec2& size) const;
    bool mousePressed() const { return m_mouseDown && !m_mouseDownPrevious; }

    Shader m_shader;
    gl::Texture m_atlasTexture{gl::TextureDeleter{}};
    gl::Buffer m_vertexBuffer{gl::BufferDeleter{}};
    gl::VertexArray m_vertexArray{gl::VertexArrayDeleter{}};
    std::vector<Vertex> m_vertices;

    int m_width = 0;
    int m_height = 0;
    float m_lineHeight = 19.0f;

    std::vector<unsigned char> m_atlasPixels;
    int m_atlasWidth = 0;
    int m_atlasHeight = 0;
    int m_atlasPenX = 0;
    int m_atlasPenY = 0;
    int m_atlasShelfHeight = 0;
    bool m_atlasDirty = false;
    std::unordered_map<unsigned int, Glyph> m_glyphs;

    void* m_textSurface = nullptr; // 定义在 .cpp 里的 GDI 表面

    glm::vec2 m_mouse{0.0f, 0.0f};
    bool m_mouseDown = false;
    bool m_mouseDownPrevious = false;
    const void* m_activeSlider = nullptr;

    bool m_panelOpen = false;
    glm::vec4 m_panelRect{0.0f, 0.0f, 0.0f, 0.0f};
    float m_cursorY = 0.0f;
    float m_contentLeft = 0.0f;
    float m_contentRight = 0.0f;
    float m_rowHeight = 24.0f;
};

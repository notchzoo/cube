#pragma once

#include <memory>
#include <string>
#include <glad/glad.h>
#include "Shader.h"

/// [速度显示] 独立屏幕叠加层；使用系统字体，无需新增字体文件或第三方 UI 库。
/// Application::loadShaders 初始化，renderScene 最后绘制，releaseResources 在上下文销毁前释放。
class Speedometer
{
public:
    Speedometer();
    ~Speedometer();
    Speedometer(const Speedometer&)=delete;
    Speedometer& operator=(const Speedometer&)=delete;

    bool initialize(const std::string& assetDirectory);
    void render(int framebufferWidth,int framebufferHeight,float speedKmh);
    void reset();

private:
    struct TextSurface;
    void updateText(float speedKmh);
    std::unique_ptr<TextSurface> m_text;
    Shader m_shader;
    GLuint m_vao=0,m_vbo=0,m_texture=0;
    std::wstring m_lastValue;
};

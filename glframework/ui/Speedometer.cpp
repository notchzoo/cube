#include "Speedometer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr int kWidth=240,kHeight=96;
}

struct Speedometer::TextSurface
{
    HDC dc=nullptr;
    HBITMAP bitmap=nullptr;
    HFONT labelFont=nullptr,valueFont=nullptr,unitFont=nullptr;
    HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;
    unsigned char* pixels=nullptr;

    bool initialize()
    {
        dc=CreateCompatibleDC(nullptr);
        if(!dc) return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=kWidth;info.bmiHeader.biHeight=-kHeight;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!bitmap || !pixels) return false;
        oldBitmap=SelectObject(dc,bitmap);
        const auto font=[](int height,int weight,const wchar_t* face)
        {
            return CreateFontW(-height,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,face);
        };
        labelFont=font(15,FW_NORMAL,L"Microsoft YaHei UI");
        valueFont=font(36,FW_SEMIBOLD,L"Segoe UI");
        unitFont=font(15,FW_NORMAL,L"Segoe UI");
        if(!labelFont || !valueFont || !unitFont) return false;
        oldFont=SelectObject(dc,labelFont);
        SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);
        return true;
    }
    ~TextSurface()
    {
        if(dc && oldFont) SelectObject(dc,oldFont);
        if(dc && oldBitmap) SelectObject(dc,oldBitmap);
        if(labelFont) DeleteObject(labelFont);
        if(valueFont) DeleteObject(valueFont);
        if(unitFont) DeleteObject(unitFont);
        if(bitmap) DeleteObject(bitmap);
        if(dc) DeleteDC(dc);
    }
};

Speedometer::Speedometer()=default;
Speedometer::~Speedometer() { reset(); }

bool Speedometer::initialize(const std::string& assetDirectory)
{
    reset();
    if(!m_shader.loadFromFiles(assetDirectory+"/shader/ui/speedometer.vert",assetDirectory+"/shader/ui/speedometer.frag")) return false;
    m_text=std::make_unique<TextSurface>();
    if(!m_text->initialize()) {reset();return false;}
    const float corners[]={0,0,1,0,1,1,0,0,1,1,0,1};
    glGenVertexArrays(1,&m_vao);glBindVertexArray(m_vao);
    glGenBuffers(1,&m_vbo);glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(corners),corners,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    glGenTextures(1,&m_texture);glBindTexture(GL_TEXTURE_2D,m_texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,kWidth,kHeight,0,GL_RED,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    updateText(0);
    glBindTexture(GL_TEXTURE_2D,0);
    return true;
}

void Speedometer::updateText(float speedKmh)
{
    std::wostringstream text;text.imbue(std::locale::classic());
    text<<std::fixed<<std::setprecision(1)<<(std::isfinite(speedKmh)?std::max(0.0f,speedKmh):0.0f);
    const auto value=text.str();
    if(value==m_lastValue) return;
    m_lastValue=value;
    std::memset(m_text->pixels,0,kWidth*kHeight*4);
    SelectObject(m_text->dc,m_text->labelFont);TextOutW(m_text->dc,18,10,L"速度",2);
    SelectObject(m_text->dc,m_text->valueFont);TextOutW(m_text->dc,16,34,value.c_str(),static_cast<int>(value.size()));
    SelectObject(m_text->dc,m_text->unitFont);TextOutW(m_text->dc,177,57,L"km/h",4);
    GdiFlush(); // GDI 可能批处理绘制；读取 DIB 前先确保文字已写入。
    std::vector<unsigned char> coverage(kWidth*kHeight);
    for(size_t i=0;i<coverage.size();++i)
        coverage[i]=std::max({m_text->pixels[i*4],m_text->pixels[i*4+1],m_text->pixels[i*4+2]});
    glBindTexture(GL_TEXTURE_2D,m_texture);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,kWidth,kHeight,GL_RED,GL_UNSIGNED_BYTE,coverage.data());
}

void Speedometer::render(int width,int height,float speedKmh)
{
    if(!m_vao || !m_shader.valid() || width<=0 || height<=0) return;
    // [速度显示] 独立保存/恢复状态，避免叠加层影响下一帧的天空、透明物体和阴影。
    GLint program,vao,activeTexture,texture,srcRgb,dstRgb,srcAlpha,dstAlpha,eqRgb,eqAlpha;
    GLboolean depthMask;
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
    const bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);
    updateText(speedKmh);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    const float scale=std::min(std::clamp(height/900.0f,.7f,2.0f),width/288.0f);
    const float margin=24*scale,panelWidth=kWidth*scale,panelHeight=kHeight*scale;
    const auto transform=glm::ortho(0.0f,static_cast<float>(width),static_cast<float>(height),0.0f)*
        glm::translate(glm::mat4(1),glm::vec3(width-margin-panelWidth,margin,0))*
        glm::scale(glm::mat4(1),glm::vec3(panelWidth,panelHeight,1));
    m_shader.use();m_shader.setMat4("uTransform",transform);
    m_shader.setVec3("uPanel",glm::vec3(kWidth,kHeight,12));m_shader.setInt("uText",0);
    glBindTexture(GL_TEXTURE_2D,m_texture);glBindVertexArray(m_vao);glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(static_cast<GLuint>(vao));glUseProgram(static_cast<GLuint>(program));
    glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(texture));glActiveTexture(static_cast<GLenum>(activeTexture));
    glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);glBlendEquationSeparate(eqRgb,eqAlpha);
    if(!blend) glDisable(GL_BLEND);
    if(depth) glEnable(GL_DEPTH_TEST);
    if(cull) glEnable(GL_CULL_FACE);
    glDepthMask(depthMask);
}

void Speedometer::reset()
{
    if(m_texture) glDeleteTextures(1,&m_texture);
    if(m_vbo) glDeleteBuffers(1,&m_vbo);
    if(m_vao) glDeleteVertexArrays(1,&m_vao);
    m_texture=m_vbo=m_vao=0;m_lastValue.clear();m_text.reset();m_shader.reset();
}

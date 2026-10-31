#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

/// 方向光的深度贴图：一个只有深度附件的帧缓冲。
/// 默认 2048x2048，纹理边界采样为白色（表示“最远”，即不遮挡）。
class ShadowMap
{
public:
    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    bool initialize(unsigned int size = 2048);
    void reset();

    /// 绑定帧缓冲、设置视口到贴图分辨率并清空深度。
    void bindForWriting();

    /// 恢复场景帧缓冲与视口。framebufferId 通常是 0（窗口），离屏渲染时传自定义 FBO。
    void unbind(GLuint framebufferId, unsigned int framebufferWidth, unsigned int framebufferHeight);

    GLuint depthTexture() const { return m_depthTexture; }
    unsigned int size() const { return m_size; }

private:
    GLuint m_fbo = 0;
    GLuint m_depthTexture = 0;
    unsigned int m_size = 0;
};
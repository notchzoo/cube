#include "ShadowMap.h"

#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
// 阴影正交投影覆盖的场景范围（世界单位）。太大会让每个纹素覆盖过宽、边缘发糊。
constexpr float kOrthoHalfExtent = 9.0f;
constexpr float kLightCameraDistance = 18.0f;
constexpr float kDepthNear = 15.0f;
constexpr float kDepthFar = 21.0f;
}

ShadowMap::~ShadowMap()
{
    reset();
}

bool ShadowMap::initialize(unsigned int size)
{
    reset();

    m_size = size;

    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_depthTexture);

    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(size), static_cast<GLsizei>(size),
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    // 手动做 3x3 比较，采样必须用 NEAREST，否则双线性插值会造出并不存在的中间深度。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const GLfloat whiteBorder[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, whiteBorder);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "[ShadowMap] framebuffer incomplete, status: 0x" << std::hex << status << std::dec << '\n';
        reset();
        return false;
    }

    return true;
}

void ShadowMap::reset()
{
    if (m_fbo != 0)
    {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_depthTexture != 0)
    {
        glDeleteTextures(1, &m_depthTexture);
        m_depthTexture = 0;
    }
    m_size = 0;
}

void ShadowMap::bindForWriting()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::unbind(GLuint framebufferId, unsigned int framebufferWidth, unsigned int framebufferHeight)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);
    glViewport(0, 0, static_cast<GLsizei>(framebufferWidth), static_cast<GLsizei>(framebufferHeight));
}

glm::mat4 ShadowMap::lightSpaceMatrix(const glm::vec3& lightDirection) const
{
    // 立方体中心附近的场景中心：阴影相机瞄准这里。
    const glm::vec3 sceneCenter(0.0f, 1.0f, 0.0f);

    const float length = glm::length(lightDirection);
    const glm::vec3 direction = (length < 1e-5f) ? glm::vec3(0.0f, -1.0f, 0.0f) : lightDirection / length;

    // 沿光线反向后退得到虚拟相机位置；它不代表灯光有实体位置。
    const glm::vec3 eye = sceneCenter - direction * kLightCameraDistance;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(direction, up)) > 0.99f)
    {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::mat4 lightView = glm::lookAt(eye, sceneCenter, up);
    const glm::mat4 lightProjection = glm::ortho(-kOrthoHalfExtent, kOrthoHalfExtent,
                                                 -kOrthoHalfExtent, kOrthoHalfExtent,
                                                 kDepthNear, kDepthFar);
    return lightProjection * lightView;
}
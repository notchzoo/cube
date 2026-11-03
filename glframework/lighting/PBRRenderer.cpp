#include "PBRRenderer.h"

#include <algorithm>
#include <string>

#include "Mesh.h"
#include "Texture2D.h"

namespace
{
// uniform 名称集中在这里，GLSL 侧必须保持一致。
constexpr const char* kProjection = "uProjection";
constexpr const char* kView = "uView";
constexpr const char* kCameraPosition = "uCameraPosition";
constexpr const char* kLightDirection = "uLightDirection";
constexpr const char* kLightRadiance = "uLightRadiance";
constexpr const char* kModel = "uModel";
constexpr const char* kAlbedo = "uAlbedo";
constexpr const char* kMetallic = "uMetallic";
constexpr const char* kRoughness = "uRoughness";
constexpr const char* kAo = "uAo";
constexpr const char* kOpacity = "uOpacity";
constexpr const char* kLightSpaceMatrix = "uLightSpaceMatrix";
constexpr const char* kShadowMap = "uShadowMap";
constexpr const char* kUseShadowMap = "uUseShadowMap";
constexpr const char* kReflectionStrength = "uReflectionStrength";
constexpr const char* kShadowStrength = "uShadowStrength";
constexpr const char* kPipCount = "uPipCount";
constexpr const char* kMaterialMode = "uMaterialMode";
constexpr const char* kUseAlbedoMap = "uUseAlbedoMap";
constexpr const char* kAlbedoTop = "uAlbedoTop";
constexpr const char* kAlbedoBottom = "uAlbedoBottom";
constexpr const char* kUseAlbedoSide = "uUseAlbedoSide";
constexpr const char* kAlbedoSide = "uAlbedoSide";
}

PBRRenderer::~PBRRenderer()
{
    reset();
}

bool PBRRenderer::initialize(const std::string& assetDirectory)
{
    const std::string vertexPath = assetDirectory + "/shader/pbr/pbr.vert";
    const std::string fragmentPath = assetDirectory + "/shader/pbr/pbr.frag";
    return m_shader.loadFromFiles(vertexPath, fragmentPath);
}

void PBRRenderer::reset()
{
    m_shader.reset();
}

void PBRRenderer::beginFrame(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                             const glm::vec3& cameraPosition, const DirectionalLight& light,
                             const glm::mat4& lightSpaceMatrix, GLuint shadowDepthTexture, bool shadowsEnabled,
                             float reflectionStrength, float shadowStrength)
{
    if (!m_shader.valid())
    {
        return;
    }

    m_shader.use();
    m_shader.setMat4(kProjection, projectionMatrix);
    m_shader.setMat4(kView, viewMatrix);
    m_shader.setVec3(kCameraPosition, cameraPosition);

    // 光方向按“传播方向”传入，着色器里取反得到表面指向光源的 L。
    m_shader.setVec3(kLightDirection, light.normalizedDirection());
    m_shader.setVec3(kLightRadiance, light.radiance());

    m_shader.setMat4(kLightSpaceMatrix, lightSpaceMatrix);
    m_shader.setFloat(kReflectionStrength, reflectionStrength);
    m_shader.setFloat(kShadowStrength, std::clamp(shadowStrength, 0.0f, 1.0f));

    const bool useShadowMap = shadowsEnabled && shadowDepthTexture != 0;
    m_shader.setInt(kUseShadowMap, useShadowMap ? 1 : 0);
    if (useShadowMap)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
        m_shader.setInt(kShadowMap, 0);
    }
}

void PBRRenderer::draw(const Mesh& mesh, const glm::mat4& modelMatrix, const PBRMaterial& material, int pipCount,
                       int firstIndex, int indexCount)
{
    if (!m_shader.valid() || !mesh.valid())
    {
        return;
    }

    const PBRMaterial sanitized = material.clamped();

    m_shader.setMat4(kModel, modelMatrix);
    m_shader.setVec3(kAlbedo, sanitized.albedo);
    m_shader.setFloat(kMetallic, sanitized.metallic);
    m_shader.setFloat(kRoughness, sanitized.roughness);
    m_shader.setFloat(kAo, sanitized.ao);
    m_shader.setFloat(kOpacity, sanitized.opacity);
    m_shader.setInt(kPipCount, std::clamp(pipCount, 0, 6));
    m_shader.setInt(kMaterialMode, sanitized.materialMode);

    // 单位 0 留给阴影贴图；livery 使用 1/2。
    const bool useAlbedoMap =
        sanitized.albedoTop != nullptr && sanitized.albedoTop->valid() && sanitized.albedoBottom != nullptr &&
        sanitized.albedoBottom->valid();
    m_shader.setInt(kUseAlbedoMap, useAlbedoMap ? 1 : 0);
    if (useAlbedoMap)
    {
        sanitized.albedoTop->bind(1);
        sanitized.albedoBottom->bind(2);
        m_shader.setInt(kAlbedoTop, 1);
        m_shader.setInt(kAlbedoBottom, 2);
    }

    const bool useAlbedoSide = useAlbedoMap && sanitized.albedoSide != nullptr && sanitized.albedoSide->valid();
    m_shader.setInt(kUseAlbedoSide, useAlbedoSide ? 1 : 0);
    if (useAlbedoSide)
    {
        sanitized.albedoSide->bind(3);
        m_shader.setInt(kAlbedoSide, 3);
    }

    if (indexCount > 0)
    {
        mesh.drawRange(firstIndex, indexCount);
    }
    else
    {
        mesh.draw();
    }
}
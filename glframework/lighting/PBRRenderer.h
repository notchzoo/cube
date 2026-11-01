#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "DirectionalLight.h"
#include "PBRMaterial.h"
#include "Shader.h"

class Mesh;

/// 持有 PBR Shader 与全部 uniform 命名。
/// beginFrame() 设置一次每帧常量；draw() 设置一次材质 + 模型矩阵并绘制一个 Mesh。
class PBRRenderer
{
public:
    PBRRenderer() = default;
    ~PBRRenderer();

    PBRRenderer(const PBRRenderer&) = delete;
    PBRRenderer& operator=(const PBRRenderer&) = delete;

    bool initialize(const std::string& assetDirectory);
    void reset();

    /// shadowDepthTexture 为 0 或 shadowsEnabled 为 false 时，直射光不受阴影影响。
    /// shadowStrength 是投射者的不透明度：半透明投射者只遮蔽相应比例的直射光。
    void beginFrame(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPosition,
                    const DirectionalLight& light, const glm::mat4& lightSpaceMatrix, GLuint shadowDepthTexture,
                    bool shadowsEnabled, float reflectionStrength, float shadowStrength);

    /// pipCount 大于 0 时在面内程序化绘制骰子点数（1..6），0 表示普通平面。
    /// indexCount 为 0 表示绘制整个 Mesh；否则只绘制 [firstIndex, firstIndex + indexCount)。
    void draw(const Mesh& mesh, const glm::mat4& modelMatrix, const PBRMaterial& material, int pipCount = 0,
              int firstIndex = 0, int indexCount = 0);

    /// 供热重载与统计读取底层程序。
    Shader& shader() { return m_shader; }
    const Shader& shader() const { return m_shader; }

private:
    Shader m_shader;
};
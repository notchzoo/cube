#pragma once

#include <string>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Shader.h"

/// 太阳的外观参数。改这里就能换"材质"，不需要碰别的文件。
struct SunStyle
{
    glm::vec3 tint{1.0f, 0.96f, 0.88f};
    float radiance = 26.0f;    // 相对亮度：越大核心越白、光晕越实
    float coreDegrees = 1.4f;  // 日盘角半径（度）
    float glowDegrees = 20.0f; // 整个 billboard 的半张角（度）
    int rayCount = 12;         // 放射光芒的数量
    float raySharpness = 7.0f; // 越大光芒越细
    float rayStrength = 0.55f; // 光芒强度
};

/// 独立的"太阳"图层。
///
/// 设计目标（对应三条要求）：
///   1. 兼容性强：只用 GL 3.3 core 与现有的 Shader 类，不引入任何第三方库；
///   2. 依赖小：不修改 SkyDome / PBRRenderer / 任何现有着色器，删掉 render() 调用即可整体移除；
///   3. 可拓展 / 换材质：程序化外观由 SunStyle 控制，也可以 setTextureFromBmp() 换成贴图
///      （约定：BMP 的亮度当 alpha，白不透明、黑透明）。
///
/// 它只是一个朝向相机、放在"无穷远方向"上的四边形；因为位置由方向而不是坐标决定，
/// 相机平移时太阳不动，只有旋转会让它移动。
class SunSprite
{
public:
    SunSprite() = default;
    ~SunSprite();

    SunSprite(const SunSprite&) = delete;
    SunSprite& operator=(const SunSprite&) = delete;

    /// 只加载自己的 shader；缺文件时返回 false，调用方可以降级为"不画太阳"。
    bool initialize(const std::string& assetDirectory);
    void reset();

    void setStyle(const SunStyle& style) { m_style = style; }
    const SunStyle& style() const { return m_style; }

    /// 用 24/32 位 BMP 贴图替换程序化材质。失败时保持当前材质并返回 false。
    bool setTextureFromBmp(const std::string& path);
    void clearTexture();
    bool usingTexture() const { return m_texture != 0; }

    /// sunDirection：从场景指向太阳的单位向量（= 光传播方向取反）。
    /// 必须在天空之后、不透明几何之前调用。
    void render(const glm::vec3& cameraPosition, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                const glm::vec3& sunDirection);

    /// 供热重载与统计读取底层程序。
    Shader& shader() { return m_shader; }
    const Shader& shader() const { return m_shader; }

private:
    void createQuad();
    void uploadTexture(const unsigned char* rgba, int width, int height);

    Shader m_shader;
    SunStyle m_style;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_texture = 0;
};
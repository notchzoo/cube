#pragma once

#include <string>

#include <glm/glm.hpp>

#include "Mesh.h"
#include "Shader.h"

/// 程序化天空：代码生成的 UV 球，从内部观看，深度写入关闭。
/// 只随相机旋转移动，不随相机平移。
class SkyDome
{
public:
    SkyDome() = default;
    ~SkyDome();

    SkyDome(const SkyDome&) = delete;
    SkyDome& operator=(const SkyDome&) = delete;

    bool initialize(const std::string& assetDirectory);
    void reset();

    /// 必须在所有不透明与半透明几何之前调用。
    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;

    /// 供热重载与统计读取底层程序。
    Shader& shader() { return m_shader; }
    const Shader& shader() const { return m_shader; }

private:
    void createSphere(float radius);

    Mesh m_sphere;
    Shader m_shader;
};
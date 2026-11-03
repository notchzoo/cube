#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "PBRMaterial.h"

/// 一个 glTF primitive 的 CPU 数据，还没有上传到 GPU。
///
/// 顶点属性顺序与 Mesh 保持一致：position / color / normal + uint32 索引。
/// 缺失 NORMAL 时由加载器按三角形面积加权补出平滑法线；缺失 COLOR_0 时填白，
/// 与 PBR 的 `uAlbedo * vColor` 组合即可表达纯色材质。
struct GltfPartData
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> colors;
    std::vector<unsigned int> indices;
    PBRMaterial material;
    glm::mat4 nodeTransform{1.0f};
};

/// 一份 glTF 文件的 CPU 结果。bounds 是应用节点变换后的世界空间 AABB。
struct GltfAsset
{
    std::vector<GltfPartData> parts;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
};

/// 解析 .glb / .gltf 为三角网格；不需要 OpenGL 上下文，可以直接进 ctest。
///
/// 只支持当前渲染路径能表达的子集：静态网格、节点变换、PBR metallic-roughness 参数。
/// 贴图、skin、morph、animation 打印警告后跳过；Draco/meshopt 压缩几何直接失败。
bool loadGltf(const std::string& path, GltfAsset& asset, std::string& error);

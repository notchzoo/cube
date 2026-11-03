#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

#include "GltfLoader.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>

#include <glm/gtc/type_ptr.hpp>

namespace
{
const glm::vec3 kDefaultAlbedo(0.62f, 0.62f, 0.62f);
constexpr float kDefaultMetallic = 0.30f;
constexpr float kDefaultRoughness = 0.45f;

std::string resultName(cgltf_result result)
{
    switch (result)
    {
    case cgltf_result_success:
        return "success";
    case cgltf_result_data_too_short:
        return "data too short";
    case cgltf_result_unknown_format:
        return "unknown format";
    case cgltf_result_invalid_json:
        return "invalid JSON";
    case cgltf_result_invalid_gltf:
        return "invalid glTF";
    case cgltf_result_invalid_options:
        return "invalid options";
    case cgltf_result_file_not_found:
        return "file not found";
    case cgltf_result_io_error:
        return "I/O error";
    case cgltf_result_out_of_memory:
        return "out of memory";
    default:
        return "unknown error";
    }
}

bool hasRequiredExtension(const cgltf_data* data, const char* name)
{
    for (cgltf_size i = 0; i < data->extensions_required_count; ++i)
    {
        if (data->extensions_required[i] != nullptr && std::strcmp(data->extensions_required[i], name) == 0)
            return true;
    }
    return false;
}

bool readVec3Accessor(const cgltf_accessor* accessor, std::vector<glm::vec3>& out)
{
    if (accessor == nullptr || accessor->type != cgltf_type_vec3 || accessor->count == 0)
        return false;

    out.resize(accessor->count);
    const cgltf_size valueCount = accessor->count * 3;
    const cgltf_size written = cgltf_accessor_unpack_floats(
        accessor, reinterpret_cast<cgltf_float*>(out.data()), valueCount);
    return written == valueCount;
}

bool readIndices(const cgltf_accessor* accessor, std::vector<unsigned int>& out)
{
    if (accessor == nullptr || accessor->count == 0)
        return false;

    out.resize(accessor->count);
    const cgltf_size written = cgltf_accessor_unpack_indices(
        accessor, out.data(), sizeof(unsigned int), accessor->count);
    return written == accessor->count;
}

void readColors(const cgltf_accessor* accessor, std::vector<glm::vec3>& out)
{
    out.assign(accessor != nullptr ? accessor->count : 0, glm::vec3(1.0f));
    if (accessor == nullptr || cgltf_num_components(accessor->type) < 3)
        return;

    for (cgltf_size i = 0; i < accessor->count; ++i)
    {
        cgltf_float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        if (cgltf_accessor_read_float(accessor, i, color, 4))
            out[i] = glm::vec3(color[0], color[1], color[2]);
    }
}

// 面积加权（叉积长度天然带权）的平滑法线；模型没有 NORMAL 时必须补。
void computeSmoothNormals(const std::vector<glm::vec3>& positions, const std::vector<unsigned int>& indices,
                          std::vector<glm::vec3>& normals)
{
    normals.assign(positions.size(), glm::vec3(0.0f));
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const unsigned int i0 = indices[i];
        const unsigned int i1 = indices[i + 1];
        const unsigned int i2 = indices[i + 2];
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size())
            continue;

        const glm::vec3 faceNormal = glm::cross(positions[i1] - positions[i0], positions[i2] - positions[i0]);
        normals[i0] += faceNormal;
        normals[i1] += faceNormal;
        normals[i2] += faceNormal;
    }

    for (glm::vec3& normal : normals)
    {
        const float lengthSquared = glm::dot(normal, normal);
        normal = lengthSquared > 1e-20f ? normal / std::sqrt(lengthSquared) : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

PBRMaterial materialFrom(const cgltf_material* source)
{
    // 本示例资源没有材质：用中性灰金属外观，避免 glTF 默认 metallic=1/roughness=1 的镜面感。
    PBRMaterial material{kDefaultAlbedo, kDefaultMetallic, kDefaultRoughness, 1.0f, 1.0f};
    if (source == nullptr)
        return material.clamped();

    if (source->has_pbr_metallic_roughness)
    {
        const cgltf_pbr_metallic_roughness& pbr = source->pbr_metallic_roughness;
        material.albedo = glm::vec3(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]);
        material.metallic = pbr.metallic_factor;
        material.roughness = pbr.roughness_factor;
        if (source->alpha_mode == cgltf_alpha_mode_blend)
            material.opacity = pbr.base_color_factor[3];
    }
    return material.clamped();
}

glm::mat4 nodeWorldMatrix(const cgltf_node* node)
{
    cgltf_float matrix[16] = {};
    cgltf_node_transform_world(node, matrix);
    return glm::make_mat4(matrix);
}

bool appendPrimitive(const cgltf_primitive& primitive, const glm::mat4& nodeTransform,
                     std::vector<GltfPartData>& parts, std::string& error)
{
    if (primitive.type != cgltf_primitive_type_triangles)
        return true; // 只画三角形；点/线跳过，不算失败。
    if (primitive.has_draco_mesh_compression)
    {
        error = "KHR_draco_mesh_compression is not supported";
        return false;
    }
    if (primitive.targets_count > 0)
        std::cerr << "[glTF] morph targets ignored; base mesh is used\n";

    const cgltf_accessor* position = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    if (position == nullptr)
        return true;

    GltfPartData part;
    part.nodeTransform = nodeTransform;
    part.material = materialFrom(primitive.material);

    if (!readVec3Accessor(position, part.positions) || part.positions.empty())
        return true;

    if (primitive.indices != nullptr)
    {
        if (!readIndices(primitive.indices, part.indices))
            return true;
    }
    else
    {
        part.indices.resize(part.positions.size());
        for (size_t i = 0; i < part.indices.size(); ++i)
            part.indices[i] = static_cast<unsigned int>(i);
    }

    if (part.indices.size() % 3 != 0)
    {
        std::cerr << "[glTF] index count is not a multiple of 3; trailing indices dropped\n";
        part.indices.resize(part.indices.size() - part.indices.size() % 3);
    }
    if (part.indices.empty())
        return true;

    for (unsigned int index : part.indices)
    {
        if (index >= part.positions.size())
        {
            error = "index out of range in primitive";
            return false;
        }
    }

    const cgltf_accessor* normal = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
    if (normal == nullptr || !readVec3Accessor(normal, part.normals) || part.normals.size() != part.positions.size())
        computeSmoothNormals(part.positions, part.indices, part.normals);

    const cgltf_accessor* color = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0);
    readColors(color, part.colors);
    if (part.colors.size() != part.positions.size())
        part.colors.assign(part.positions.size(), glm::vec3(1.0f));

    parts.push_back(std::move(part));
    return true;
}

bool visitNode(const cgltf_node* node, std::vector<GltfPartData>& parts, std::string& error)
{
    const glm::mat4 world = nodeWorldMatrix(node);
    if (node->mesh != nullptr)
    {
        for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i)
        {
            if (!appendPrimitive(node->mesh->primitives[i], world, parts, error))
                return false;
        }
    }

    for (cgltf_size i = 0; i < node->children_count; ++i)
    {
        if (!visitNode(node->children[i], parts, error))
            return false;
    }
    return true;
}

void updateBounds(const std::vector<GltfPartData>& parts, glm::vec3& min, glm::vec3& max)
{
    const float inf = std::numeric_limits<float>::infinity();
    min = glm::vec3(inf);
    max = glm::vec3(-inf);

    for (const GltfPartData& part : parts)
    {
        for (const glm::vec3& position : part.positions)
        {
            const glm::vec3 world = glm::vec3(part.nodeTransform * glm::vec4(position, 1.0f));
            min = glm::min(min, world);
            max = glm::max(max, world);
        }
    }

    if (parts.empty())
    {
        min = glm::vec3(0.0f);
        max = glm::vec3(0.0f);
    }
}
} // namespace

bool loadGltf(const std::string& path, GltfAsset& asset, std::string& error)
{
    asset = GltfAsset{};
    error.clear();

    cgltf_options options{};
    cgltf_data* rawData = nullptr;
    const cgltf_result parseResult = cgltf_parse_file(&options, path.c_str(), &rawData);
    if (parseResult != cgltf_result_success)
    {
        error = "cgltf_parse_file(" + path + "): " + resultName(parseResult);
        return false;
    }

    const std::unique_ptr<cgltf_data, void (*)(cgltf_data*)> data(rawData, &cgltf_free);

    const cgltf_result bufferResult = cgltf_load_buffers(&options, data.get(), path.c_str());
    if (bufferResult != cgltf_result_success)
    {
        error = "cgltf_load_buffers: " + resultName(bufferResult);
        return false;
    }

    if (cgltf_validate(data.get()) != cgltf_result_success)
    {
        error = "cgltf_validate failed: " + path;
        return false;
    }

    if (hasRequiredExtension(data.get(), "KHR_draco_mesh_compression") ||
        hasRequiredExtension(data.get(), "EXT_meshopt_compression"))
    {
        error = "compressed geometry (Draco/meshopt) is not supported";
        return false;
    }

    if (data->animations_count > 0)
        std::cerr << "[glTF] animations ignored (" << data->animations_count << " clip(s))\n";
    if (data->skins_count > 0)
        std::cerr << "[glTF] skins ignored; bind pose is used\n";
    if (data->textures_count > 0)
        std::cerr << "[glTF] textures ignored (" << data->textures_count << " texture(s))\n";

    const cgltf_scene* scene = data->scene;
    if (scene == nullptr && data->scenes_count > 0)
        scene = &data->scenes[0];
    if (scene == nullptr)
    {
        error = "glTF has no scene";
        return false;
    }

    for (cgltf_size i = 0; i < scene->nodes_count; ++i)
    {
        if (!visitNode(scene->nodes[i], asset.parts, error))
            return false;
    }

    if (asset.parts.empty())
    {
        error = "no triangle primitives in " + path;
        return false;
    }

    updateBounds(asset.parts, asset.boundsMin, asset.boundsMax);
    return true;
}

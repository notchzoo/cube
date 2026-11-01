#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "SceneObject.h"

/// 场景 = 一组 SceneObject。只负责"有哪些对象、按什么顺序产出绘制项"，
/// 不持有渲染器，也不关心材质/光照的具体内容。
class Scene
{
public:
    Scene() = default;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    template <typename T, typename... Args> T& add(Args&&... args)
    {
        auto object = std::make_unique<T>(std::forward<Args>(args)...);
        T& reference = *object;
        m_objects.push_back(std::move(object));
        return reference;
    }

    void clear() { m_objects.clear(); }

    void appendDrawItems(std::vector<DrawItem>& items) const;
    void appendShadowItems(std::vector<DrawItem>& items) const;

    /// 阴影强度近似：取所有投射者里最大的不透明度（单方向光、单张深度图）。
    float shadowCasterOpacity() const;

    size_t objectCount() const { return m_objects.size(); }

private:
    std::vector<std::unique_ptr<SceneObject>> m_objects;
};
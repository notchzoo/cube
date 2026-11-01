#pragma once

#include <vector>

#include "DrawItem.h"

/// 场景对象的最小接口：只产出绘制项，不持有渲染器、不碰 GL 状态。
///
/// 这样 Application 可以统一遍历场景，而 DiceObject / MeshObject
/// 之间互不依赖，也不需要知道彼此的材质或光照细节。
class SceneObject
{
public:
    virtual ~SceneObject() = default;

    /// 主 pass：可能产出多个 DrawItem（骰子每个面一个）。
    virtual void appendDrawItems(std::vector<DrawItem>& items) const = 0;

    /// 深度 pass：默认与主 pass 相同，单面接收者之类可以返回空。
    virtual void appendShadowItems(std::vector<DrawItem>& items) const { appendDrawItems(items); }

    /// 是否参与阴影投射（决定它是否写深度、以及阴影强度的近似取值）。
    virtual bool castsShadow() const { return true; }

    /// 整体不透明度：用于透明排序与阴影强度近似。
    virtual float opacity() const { return 1.0f; }
};
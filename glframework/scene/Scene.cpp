#include "Scene.h"

#include <algorithm>

void Scene::appendDrawItems(std::vector<DrawItem>& items) const
{
    for (const auto& object : m_objects)
    {
        if (object)
        {
            object->appendDrawItems(items);
        }
    }
}

void Scene::appendShadowItems(std::vector<DrawItem>& items) const
{
    for (const auto& object : m_objects)
    {
        if (object && object->castsShadow())
        {
            object->appendShadowItems(items);
        }
    }
}

float Scene::shadowCasterOpacity() const
{
    float strongest = 0.0f;
    bool found = false;
    for (const auto& object : m_objects)
    {
        if (!object || !object->castsShadow())
        {
            continue;
        }
        strongest = found ? std::max(strongest, object->opacity()) : object->opacity();
        found = true;
    }
    return found ? strongest : 1.0f;
}
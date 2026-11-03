#include "GltfObject.h"

void GltfObject::appendDrawItems(std::vector<DrawItem>& items) const
{
    for (const Part& part : m_parts)
    {
        if (!part.mesh || !part.mesh->valid())
            continue;

        DrawItem item;
        item.mesh = part.mesh.get();
        item.model = m_placement * part.nodeTransform;
        item.material = part.material;
        // 闭合机体：阴影 pass 只画背面；当前默认关闭投影，打开后可直接复用。
        item.shadowCull = DrawItem::FaceCull::Front;
        items.push_back(item);
    }
}

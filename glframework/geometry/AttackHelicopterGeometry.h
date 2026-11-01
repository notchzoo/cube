#pragma once

#include <memory>
#include "MeshBuilder.h"

class Mesh;
class MeshBuilder;

/// [几何生成] 程序化 AH-64D 外形；局部 +X 机头、+Y 上、+Z 右，一单位约两米。
/// 只生成网格与转轴，不读取按键、不推进动画、不积分物理。
/// 默认调用：Application::createScene -> createMeshes -> AttackHelicopterObject。
/// 参考依据及拟合限制见 docs/AH64D_REFERENCE.md。
namespace AttackHelicopterGeometry
{
enum class Loadout
{
    Mixed,  // 默认：两侧外挂点导弹，左内火箭巢，右内油箱
    Combat, // 两侧外挂点导弹，两个内挂点均为火箭巢
    Ferry   // 两侧外挂点导弹，两个内挂点均为副油箱
};

struct Parts
{
    MeshBuilder body;       // 固定机身，包含雷达、垂尾、武器和起落架
    MeshBuilder mainRotor;  // 相对主轴原点的主旋翼网格
    MeshBuilder tailBlade;  // 一片径向叶片，绘制四次组成剪刀式尾桨
    MeshBuilder stabilator; // 相对前缘铰链原点的水平尾翼
};

struct Meshes
{
    std::shared_ptr<Mesh> body, mainRotor, tailBlade, stabilator;
};

// [转轴定义] 返回已完成垂直比例修正的机体坐标；修改后须同步核对动画与几何。
glm::vec3 mainRotorPivot();
glm::vec3 tailRotorPivot();
glm::vec3 stabilatorPivot();
glm::vec3 stabilatorAxis();
float tailBladeRestAngle(int blade); // 弧度，保持 55°/125° 剪刀布局

/// CPU 生成入口，无需 GL 上下文；可动网格归一到各自铰链原点，供动画与单元检查使用。
Parts buildParts(Loadout loadout = Loadout::Mixed);
/// 默认动画入口：建立 GL 上下文后调用，上传四份 Mesh；Scene 必须在销毁上下文前释放它们。
Meshes createMeshes(Loadout loadout = Loadout::Mixed);

/// 静态合并接口：把四份部件按零动画姿态合到 builder，用于几何检查或静态道具。
void appendTo(MeshBuilder& builder, Loadout loadout = Loadout::Mixed);

/// 上传一份静态合并网格；若需旋翼/尾翼动画，应使用 createMeshes。
std::shared_ptr<Mesh> createMesh(Loadout loadout = Loadout::Mixed);
}

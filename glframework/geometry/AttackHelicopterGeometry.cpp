#include "AttackHelicopterGeometry.h"

#include <cmath>
#include <iostream>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include "Mesh.h"
#include "MeshBuilder.h"

namespace
{
const glm::vec3 olive(0.19f, 0.22f, 0.13f);
const glm::vec3 armor(0.25f, 0.28f, 0.18f);
const glm::vec3 dark(0.035f, 0.043f, 0.038f);
const glm::vec3 metal(0.32f, 0.34f, 0.29f);
const glm::vec3 glass(0.10f, 0.24f, 0.31f);

glm::mat4 pose(glm::vec3 p, float degrees = 0.0f, glm::vec3 axis = glm::vec3(0, 1, 0))
{
    return glm::rotate(glm::translate(glm::mat4(1), p), glm::radians(degrees), axis);
}

void beam(MeshBuilder& b, glm::vec3 a, glm::vec3 c, float radius, glm::vec3 color)
{
    const glm::vec3 direction = glm::normalize(c - a);
    const glm::vec3 reference = std::abs(direction.y) > 0.95f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
    const glm::vec3 x = glm::normalize(glm::cross(direction, reference));
    glm::mat4 transform(1);
    transform[0] = glm::vec4(x, 0);
    transform[1] = glm::vec4(direction, 0);
    transform[2] = glm::vec4(glm::cross(x, direction), 0);
    transform[3] = glm::vec4((a + c) * 0.5f, 1);
    b.addSmoothCylinder(transform, radius, glm::length(c - a), 20, color);
}

// All stores use the same four hardpoints. Local units = metres / 2.
// The inboard span and longitudinal datum are fitted from public drawings;
// they are not claimed to be manufacturer installation coordinates.
constexpr float inboardZ = .80f;
constexpr float outboardZ = 1.21f;
constexpr float storesX = -.30f;
constexpr float storesY = -.38f;

// [几何细节] 武器/油箱均在 buildParts 中按四个挂点放置；这些函数不处理挂点选择。
void rocketPod(MeshBuilder& b, glm::vec3 p)
{
    b.addSmoothCylinder(pose(p,90,{0,0,1}),.132f,.85f,64,olive);
    for (float x : {-.43f,.43f})
        b.addSmoothCylinder(pose(p+glm::vec3(x,0,0),90,{0,0,1}),.137f,.025f,32,metal);
    // M261: 19 tubes in a hexagonal 3/4/5/4/3 packing, not the M260 seven-shot face.
    for (int row = -2; row <= 2; ++row)
    {
        const int count = 5-std::abs(row);
        for (int col = 0; col < count; ++col)
        {
            const float y = row*.0433f;
            const float z = (col-(count-1)*.5f)*.05f;
            b.addSmoothCylinder(pose(p+glm::vec3(.447f,y,z),90,{0,0,1}),.021f,.014f,10,dark);
            b.addSmoothCylinder(pose(p+glm::vec3(-.447f,y,z),90,{0,0,1}),.021f,.014f,10,dark);
        }
    }
    for (float x : {-.25f,.22f})
        b.addSmoothCylinder(pose(p+glm::vec3(x,0,0),90,{0,0,1}),.135f,.018f,32,armor);
}

void hellfireRack(MeshBuilder& b, glm::vec3 p)
{
    // M299 envelope: 599 mm wide / 737 mm high. AGM-114: 1.63 m x .178 m.
    b.addBox(pose(p+glm::vec3(0,.055f,0)),{.22f,.12f,.028f},metal);
    for (float y : {.055f,-.145f})
    {
        b.addBox(pose(p+glm::vec3(0,y+.045f,0)),{.25f,.018f,.115f},armor);
        for (float z : {-.105f,.105f})
        {
            const auto missile = p+glm::vec3(0,y,z);
            b.addBox(pose(missile+glm::vec3(-.03f,.05f,0)),{.31f,.015f,.021f},metal);
            b.addSmoothLoft(pose(missile),{{-.4075f,.026f,.026f,0},{-.36f,.0445f,.0445f,0},
                {.29f,.0445f,.0445f,0},{.365f,.032f,.032f,0},{.4075f,.013f,.013f,0}},32,dark);
            b.addSmoothCylinder(pose(missile+glm::vec3(.392f,0,0),90,{0,0,1}),.019f,.026f,12,glass);
            for (float x : {-.20f,.245f})
                b.addSmoothCylinder(pose(missile+glm::vec3(x,0,0),90,{0,0,1}),.0455f,.014f,16,{.62f,.49f,.15f});
            for (int fin = 0; fin < 4; ++fin)
            {
                const auto frame=pose(missile,45+fin*90.0f,{1,0,0});
                b.addBox(glm::translate(frame,{-.29f,.058f,0}),{.074f,.035f,.005f},armor);
                b.addBox(glm::translate(frame,{.08f,.056f,0}),{.05f,.027f,.004f},armor);
            }
        }
    }
}

void externalTank(MeshBuilder& b, glm::vec3 p)
{
    // ERFS 230-gallon silhouette. Tank shape/length are fitted, not a dimensioned part drawing.
    b.addSmoothLoft(pose(p),{{-.88f,.018f,.018f,0},{-.75f,.15f,.15f,0},
        {-.52f,.205f,.205f,0},{.45f,.205f,.205f,0},{.70f,.145f,.145f,0},
        {.84f,.055f,.055f,0},{.88f,.012f,.012f,0}},64,olive,12);
    for (float x : {-.34f,.30f})
    {
        b.addSmoothCylinder(pose(p+glm::vec3(x,0,0),90,{0,0,1}),.209f,.024f,32,armor);
        b.addBox(pose(p+glm::vec3(x,.21f,0)),{.065f,.07f,.06f},metal);
    }
    b.addSmoothCylinder(pose(p+glm::vec3(.16f,.204f,.015f)),.033f,.014f,16,dark);
    beam(b,p+glm::vec3(-.12f,.20f,0),p+glm::vec3(-.12f,.34f,0),.014f,metal);
    for(float x : {-.34f,.30f})
        for(int bolt=0;bolt<12;++bolt)
        {
            const float angle=glm::radians(bolt*30.0f);
            const auto point=p+glm::vec3(x,.211f*std::sin(angle),.211f*std::cos(angle));
            b.addSmoothCylinder(pose(point,90-bolt*30.0f,{1,0,0}),.006f,.008f,8,metal);
        }
}

void tandemCanopy(MeshBuilder& b)
{
    // [几何细节] 前/侧风挡与窗框；透明效果尚未启用，玻璃靠顶点色区分。
    struct Section { float x, bottom, roof, width; };
    const Section sections[]={{-.12f,.42f,1.12f,.40f},{.50f,.38f,1.10f,.38f},
        {.73f,.34f,.87f,.36f},{1.30f,.27f,.75f,.30f},{1.58f,.24f,.31f,.25f}};
    for (int i=0;i<4;++i)
    {
        const auto& a=sections[i]; const auto& c=sections[i+1];
        b.addQuad({a.x,a.roof,-a.width*.72f},{c.x,c.roof,-c.width*.72f},
            {c.x,c.roof,c.width*.72f},{a.x,a.roof,a.width*.72f},glass,{0,.4f,0});
        // The sloping front/overhead glazing is visible head-on; transverse
        // frames separate the tandem windscreens without a solid roof slab.
        beam(b,{a.x,a.roof,-a.width*.72f},{a.x,a.roof,a.width*.72f},.018f,armor);
        if(i==3) beam(b,{c.x,c.roof,-c.width*.72f},{c.x,c.roof,c.width*.72f},.018f,armor);
        for (int side : {-1,1})
        {
            const float s=static_cast<float>(side);
            const glm::vec3 al(a.x,a.bottom,s*a.width),ah(a.x,a.roof,s*a.width*.72f);
            const glm::vec3 cl(c.x,c.bottom,s*c.width),ch(c.x,c.roof,s*c.width*.72f);
            b.addQuad(al,cl,ch,ah,glass,{0,.45f,0});
            beam(b,al,cl,.024f,armor); beam(b,ah,ch,.025f,armor);
            beam(b,al,ah,.026f,armor);
            if(i==3) beam(b,cl,ch,.024f,armor);
        }
    }
    const auto& rear=sections[0];
    b.addQuad({rear.x,rear.bottom,-rear.width},{rear.x,rear.bottom,rear.width},
        {rear.x,rear.roof,rear.width*.72f},{rear.x,rear.roof,-rear.width*.72f},olive,{0,.5f,0});
}

void noseAssembly(MeshBuilder& b)
{
    // [几何细节] 机头修改入口：下颌/侧整流罩 -> 下层 TADS -> 上层 PNVS -> 风挡附件。
    // Reference silhouette: broad chin/cheeks, a lower asymmetric TADS assembly,
    // and a separate, smaller PNVS above it. Coordinates precede verticalFit.
    const glm::vec3 housing(.22f,.245f,.165f),seal(.055f,.063f,.048f);
    b.addSmoothLoft(glm::mat4(1),{{1.01f,.43f,.31f,-.11f},
        {1.35f,.49f,.33f,-.13f},{1.73f,.50f,.30f,-.15f},
        {1.96f,.46f,.26f,-.16f},{2.035f,.40f,.21f,-.17f}},64,olive,10);
    for(int side : {-1,1})
    {
        const float s=static_cast<float>(side);
        // Raised cheek lips wrap around the recessed sensor platform.
        b.addSmoothLoft(pose({0,0,s*.36f}),{{1.18f,.09f,.21f,-.04f},
            {1.43f,.13f,.24f,-.04f},{1.79f,.125f,.23f,-.07f},
            {1.99f,.075f,.17f,-.09f}},40,armor,8);
        beam(b,{1.45f,.11f,s*.39f},{1.89f,.10f,s*.40f},.024f,olive);
        b.addBox(pose({1.92f,.12f,s*.375f}),{.085f,.12f,.055f},housing);
        b.addSmoothCylinder(pose({1.96f,.075f,s*.443f},90,{1,0,0}),.072f,.035f,32,dark);
        b.addSmoothCylinder(pose({1.96f,.075f,s*.465f},90,{1,0,0}),.043f,.012f,24,metal);
        b.addSmoothCylinder(pose({1.96f,.075f,s*.474f},90,{1,0,0}),.024f,.008f,16,{.46f,.37f,.16f});

        // Side access panel, fasteners and a small service stencil.
        beam(b,{1.22f,-.24f,s*.461f},{1.77f,-.26f,s*.476f},.004f,seal);
        for(int bolt=0;bolt<6;++bolt)
            b.addSmoothCylinder(pose({1.23f+bolt*.09f,-.19f,s*.487f},90,{1,0,0}),.007f,.008f,10,metal);
        b.addSmoothCylinder(pose({1.80f,-.19f,s*.473f},90,{1,0,0}),.025f,.009f,24,dark);
        b.addSmoothCylinder(pose({1.80f,-.19f,s*.480f},90,{1,0,0}),.014f,.009f,20,metal);
        b.addBox(pose({1.50f,-.17f,s*.497f}),{.018f,.025f,.003f},{.48f,.10f,.065f});
        b.addBox(pose({1.50f,-.215f,s*.494f}),{.018f,.019f,.003f},{.57f,.58f,.52f});
    }

    // Mounting cross-shaft and yoke remain visible above the lower turret.
    b.addSmoothCylinder(pose({1.965f,.075f,0},90,{1,0,0}),.063f,.82f,32,metal);
    b.addBox(pose({1.97f,.21f,0}),{.085f,.04f,.31f},olive);
    b.addBox(pose({2.02f,.17f,-.10f}),{.085f,.04f,.16f},dark);

    // Starboard rounded sensor head. Its front cover is olive, as in the
    // supplied image; it is not a second matching forward-facing camera tube.
    b.addSmoothLoft(pose({2.075f,-.02f,.105f}),{
        {-.22f,.08f,.14f,0},{-.15f,.17f,.25f,0},{0,.21f,.30f,0},
        {.14f,.164f,.235f,0},{.205f,.087f,.124f,0},{.225f,.004f,.005f,0}},64,housing,10);
    // A slightly proud circular cover and gasket, compensated for verticalFit.
    const auto cover=pose({2.215f,-.02f,.105f})*
        glm::scale(glm::mat4(1),glm::vec3(1,1.0f/.72f,1))*
        glm::rotate(glm::mat4(1),glm::radians(90.0f),glm::vec3(0,0,1));
    b.addTorus(cover,.164f,.012f,64,12,dark);
    for(int bolt=0;bolt<8;++bolt)
    {
        const float a=glm::radians(bolt*45.0f);
        b.addSmoothCylinder(pose({2.227f,-.02f+.164f*std::sin(a)/.72f,.105f+.164f*std::cos(a)},90,{0,0,1}),
            .0045f,.009f,10,metal);
    }

    // Port day-sight housing: broad rectangular face, bevelled corners and
    // unequal optical openings, kept distinct from the round head beside it.
    b.addLoft(pose({2.045f,-.015f,-.245f}),{{-.14f,.155f,.245f,0},
        {.08f,.175f,.245f,0},{.17f,.155f,.22f,0}},8,dark);
    b.addBox(pose({2.225f,-.015f,-.245f}),{.018f,.205f,.142f},housing);
    b.addBox(pose({2.247f,-.015f,-.245f}),{.007f,.177f,.117f},seal);
    b.addBox(pose({2.258f,.018f,-.245f}),{.006f,.127f,.099f},{.12f,.13f,.105f});
    b.addSmoothCylinder(pose({2.269f,.045f,-.245f},90,{0,0,1}),.086f,.015f,48,dark);
    b.addSmoothCylinder(pose({2.28f,.045f,-.245f},90,{0,0,1}),.069f,.012f,48,{.26f,.30f,.20f});
    b.addSmoothCylinder(pose({2.288f,.055f,-.257f},90,{0,0,1}),.033f,.005f,32,glass);
    for(float z : {-.285f,-.205f})
        b.addSmoothCylinder(pose({2.269f,-.135f,z},90,{0,0,1}),.025f,.012f,24,glass);
    for(float y : {-.19f,.16f}) for(float z : {-.365f,-.125f})
        b.addSmoothCylinder(pose({2.251f,y,z},90,{0,0,1}),.006f,.009f,12,metal);

    // Upper PNVS pedestal, azimuth joint, rounded cap and shaded FLIR window.
    b.addSmoothCylinder(pose({1.825f,.285f,0}),.14f,.12f,48,dark);
    b.addSmoothCylinder(pose({1.825f,.355f,0}),.17f,.045f,48,metal);
    b.addSmoothLoft(pose({1.825f,.455f,0},90,{0,0,1}),{
        {-.095f,.145f,.145f,0},{-.06f,.175f,.175f,0},
        {.09f,.175f,.175f,0},{.13f,.135f,.135f,0}},48,olive,8);
    b.addBox(pose({2.001f,.465f,0}),{.018f,.074f,.113f},dark);
    b.addBox(pose({2.025f,.465f,0}),{.008f,.054f,.088f},glass);
    b.addBox(pose({2.012f,.550f,0}),{.052f,.013f,.13f},armor);
    for(float z : {-.185f,.185f})
        b.addSmoothCylinder(pose({1.83f,.445f,z},90,{1,0,0}),.035f,.015f,24,metal);

    // Sloping deck, windshield wipers and sensor harness tie the turrets
    // back into the cockpit instead of leaving them on the end of a tube.
    b.addBox(pose({1.65f,.265f,0},-14,{0,0,1}),{.20f,.025f,.25f},olive);
    for(float z : {-.15f,.15f})
    {
        beam(b,{1.55f,.33f,z},{1.38f,.62f,z*.88f},.006f,dark);
        beam(b,{1.39f,.61f,z*.88f},{1.31f,.70f,z*.88f},.008f,dark);
    }
    beam(b,{1.67f,.20f,-.29f},{1.87f,.20f,-.29f},.016f,dark);
    beam(b,{1.87f,.20f,-.29f},{1.93f,.11f,-.32f},.016f,dark);
}

void chinGun(MeshBuilder& b)
{
    // [几何细节] 机炮外观：安装座、俯仰支架、机匣、枪管及供弹链；不实现武器功能。
    b.addSmoothCylinder(pose({1.08f,-.49f,0}),.18f,.13f,40,armor);
    b.addSmoothCylinder(pose({1.08f,-.575f,0}),.125f,.065f,32,dark);
    for(float z : {-.115f,.115f})
    {
        beam(b,{1.03f,-.60f,z},{1.12f,-.87f,z},.028f,metal);
        beam(b,{1.23f,-.60f,z},{1.12f,-.87f,z},.025f,olive);
        b.addSmoothCylinder(pose({1.12f,-.86f,z},90,{1,0,0}),.069f,.035f,32,armor);
        b.addSmoothCylinder(pose({1.12f,-.86f,z*1.19f},90,{1,0,0}),.033f,.018f,20,metal);
    }
    b.addBox(pose({1.16f,-.93f,0}),{.205f,.10f,.10f},dark);
    b.addBox(pose({1.27f,-.96f,0}),{.15f,.065f,.075f},olive);
    const auto barrel=pose({1.56f,-.97f,0},-8,{0,0,1});
    b.addSmoothCylinder(barrel*glm::rotate(glm::mat4(1),glm::radians(90.0f),glm::vec3(0,0,1)),.028f,.67f,32,dark);
    b.addSmoothCylinder(pose({1.89f,-1.016f,0},82,{0,0,1}),.041f,.09f,32,metal);
    b.addSmoothCylinder(pose({1.939f,-1.023f,0},82,{0,0,1}),.022f,.008f,24,dark);
    for(int rib=0;rib<10;++rib)
    {
        const float t=rib/9.0f;
        const glm::vec3 p(.73f+.39f*t,-.43f-.50f*t,.16f+.025f*std::sin(t*3.14159265f));
        b.addBox(pose(p,-36,{0,0,1}),{.035f,.024f,.055f},rib%2?metal:dark);
    }
    for(float x : {1.01f,1.13f,1.25f})
        b.addSmoothCylinder(pose({x,-.94f,.108f},90,{1,0,0}),.009f,.012f,12,metal);
}
}

namespace
{
void buildAirframe(MeshBuilder& b, MeshBuilder& mainRotor, MeshBuilder& stabilator)
{
    // [几何生成] 固定部件写入 b，可动部件分别写入 mainRotor/stabilator，避免动画时机身一起转。
    // Narrow armored fuselage and raised tandem canopy, rather than a bubble cabin.
    b.addSmoothLoft(glm::mat4(1), {
        {-2.0f, .25f, .30f, .15f}, {-1.25f, .48f, .50f, .06f},
        {0, .53f, .51f, 0}, {1.15f, .46f, .43f, -.03f},
        {1.70f, .35f, .31f, -.08f}, {1.91f, .19f, .22f, -.08f}}, 64, olive,12);
    tandemCanopy(b);
    for (int side : {-1, 1})
    {
        const float s = static_cast<float>(side);
        // Twin engine nacelles with black recessed-looking intake/exhaust discs.
        b.addSmoothLoft(pose({-.65f, .57f, s*.64f}), {
            {-1.0f, .22f, .24f, 0}, {-.75f, .29f, .29f, 0},
            {.60f, .29f, .29f, 0}, {.82f, .22f, .22f, 0}}, 64, olive,12);
        b.addSmoothCylinder(pose({.18f, .57f, s*.64f}, 90, {0,0,1}), .18f, .025f, 20, dark);
        b.addSmoothCylinder(pose({-1.68f, .57f, s*.64f}, 90, {0,0,1}), .17f, .16f, 16, dark);
        b.addTorus(pose({.198f,.57f,s*.64f},90,{0,0,1}),.196f,.018f,64,12,metal);
        b.addSmoothCylinder(pose({.211f,.57f,s*.64f},90,{0,0,1}),.046f,.025f,32,armor);
        for(int blade=0;blade<16;++blade)
        {
            const auto fan=pose({.204f,.57f,s*.64f},blade*22.5f,{1,0,0});
            b.addBox(glm::translate(fan,{0,.112f,0})*
                glm::rotate(glm::mat4(1),glm::radians(28.0f),glm::vec3(0,1,0)),{.006f,.064f,.012f},metal);
        }
        b.addTorus(pose({-1.769f,.57f,s*.64f},90,{0,0,1}),.174f,.015f,48,10,armor);

        // 5.23 m wing span; four stations, no extra stores attached to the belly.
        b.addBox(pose({storesX,.02f,s*.88f}),{.37f,.052f,.4275f},olive);
        for (float z : {inboardZ,outboardZ})
        {
            b.addLoft(pose({storesX,-.12f,s*z}),{{-.17f,.045f,.14f,0},
                {.12f,.045f,.14f,0},{.24f,.018f,.10f,.04f}},8,armor);
            beam(b,{storesX,-.24f,s*z},{storesX,-.31f,s*z},.027f,metal);
        }

        // AH-64D avionics cheek fairings, access seams, steps and cooling louvers.
        b.addLoft(pose({-.38f,-.14f,s*.48f}),{{-1.13f,.10f,.20f,0},
            {-.85f,.16f,.22f,0},{.70f,.16f,.20f,0},{1.07f,.07f,.13f,0}},8,olive);
        for(float x : {-.95f,-.50f,.05f})
        {
            b.addBox(pose({x,-.13f,s*.643f}),{.006f,.145f,.006f},dark);
            b.addBox(pose({x+.16f,-.13f,s*.645f}),{.045f,.012f,.01f},metal);
        }
        for(int vent=0;vent<7;++vent)
            b.addBox(pose({-1.25f+vent*.07f,.59f,s*.918f},-12,{0,0,1}),{.015f,.105f,.005f},dark);
        for(int bolt=0;bolt<15;++bolt)
            for(float y : {-.265f,-.02f})
                b.addSmoothCylinder(pose({-1.10f+bolt*.085f,y,s*.626f},90,{1,0,0}),.007f,.012f,8,metal);
        // Canopy hinges and separate door handles retain a crisp manufactured edge.
        for(float x : {.03f,.43f,.80f,1.15f})
            b.addBox(pose({x,.40f,s*.397f}),{.045f,.014f,.015f},metal);
        b.addBox(pose({.32f,.55f,s*.37f}),{.055f,.012f,.014f},dark);
        b.addBox(pose({1.03f,.43f,s*.31f}),{.045f,.01f,.014f},dark);
        beam(b,{.68f,.08f,s*.48f},{.68f,-.09f,s*.64f},.017f,metal);
        beam(b,{.68f,-.09f,s*.64f},{1.0f,-.09f,s*.64f},.017f,metal);
        beam(b,{1.0f,-.09f,s*.64f},{1.0f,.08f,s*.43f},.017f,metal);

        // Splayed main gear, tyre and outer hub; wheel axis is local Z.
        beam(b, {.55f,-.28f,s*.34f}, {.35f,-.82f,s*.5075f}, .055f, metal);
        beam(b, {-.16f,-.32f,s*.40f}, {.35f,-.82f,s*.5075f}, .043f, olive);
    }

    // Raised, tapered tail boom, swept fin and horizontal stabilizer.
    b.addSmoothLoft(glm::mat4(1), {{-5.50f,.09f,.14f,.55f},{-4.2f,.14f,.18f,.40f},
                             {-2.3f,.22f,.24f,.20f},{-1.35f,.31f,.30f,.12f}}, 48, olive,12);
    b.addBox(pose({-5.26f,1.04f,0},-16,{0,0,1}), {.23f,.73f,.055f}, armor);
    stabilator.addBox(pose({-4.65f,.52f,0},-8,{0,1,0}), {.32f,.045f,.89f}, olive);
    beam(b, {-4.98f,.37f,0}, {-5.08f,-.75f,0}, .04f, metal);
    beam(b, {-5.22f,1.65f,0}, {-5.25f,2.02f,0}, .012f, dark);

    // Transmission, exposed rotor hub, four long blades with swept tips.
    b.addBox(pose({-.55f,.99f,0}), {.42f,.17f,.28f}, olive);
    b.addSmoothCylinder(pose({-.50f,1.38f,0}), .09f,.53f,12,metal);
    mainRotor.addSmoothCylinder(pose({-.50f,1.65f,0}), .23f,.12f,16,olive);
    for (int i = 0; i < 4; ++i)
    {
        const auto rotor = pose({-.50f,1.66f,0}, 18+i*90.0f);
        mainRotor.addBox(glm::translate(rotor,{.35f,0,0}), {.30f,.055f,.09f},metal);
        mainRotor.addBox(glm::translate(rotor,{2.0f,.015f,0}), {1.46f,.018f,.1325f},dark);
        mainRotor.addBox(glm::translate(rotor,{3.54f,.005f,-.04f}), {.1175f,.018f,.10f},dark);
        beam(mainRotor,{-.50f,1.50f,0},glm::vec3(rotor*glm::vec4(.37f,0,0,1)),.025f,metal);
    }

    // Optional on the real AH-64D; this model depicts the radar-equipped Longbow.
    b.addSmoothCylinder(pose({-.50f,1.88f,0}),.065f,.38f,16,metal);
    b.addSmoothCylinder(pose({-.50f,2.075f,0}),.25f,.12f,32,dark);
    // Rounded radome lip, with a shallow crown instead of a stack of flat discs.
    b.addSmoothLoft(pose({-.50f,2.19f,0},90,{0,0,1}),{
        {-.12f,.30f,.30f,0},{-.09f,.35f,.35f,0},{.065f,.35f,.35f,0},
        {.11f,.30f,.30f,0},{.13f,.018f,.018f,0}},64,olive,8);
    b.addBox(pose({-.50f,1.97f,0}),{.15f,.055f,.11f},armor);

    noseAssembly(b);
    chinGun(b);
    // Air-data booms and navigation lamps remain outside the canopy.
    for(int side : {-1,1})
    {
        const float s=static_cast<float>(side);
        beam(b,{.05f,.90f,s*.42f},{.22f,1.03f,s*.59f},.014f,dark);
        beam(b,{.22f,1.03f,s*.59f},{.48f,1.03f,s*.59f},.01f,metal);
        b.addBox(pose({storesX,.055f,s*1.31f}),{.04f,.025f,.018f},
            side<0?glm::vec3(.55f,.035f,.025f):glm::vec3(.025f,.40f,.12f));
    }
}
}

glm::vec3 AttackHelicopterGeometry::mainRotorPivot() { return {-.50f,1.66f*.72f,0}; }
glm::vec3 AttackHelicopterGeometry::tailRotorPivot() { return {-5.28f,1.15f*.72f,-.32f}; }
glm::vec3 AttackHelicopterGeometry::stabilatorPivot() { return {-4.34f,.52f*.72f,0}; }
glm::vec3 AttackHelicopterGeometry::stabilatorAxis() { return glm::vec3(pose({0,0,0},-8)*glm::vec4(0,0,1,0)); }
float AttackHelicopterGeometry::tailBladeRestAngle(int blade)
{
    return glm::radians(25+(blade%2)*55.0f+(blade/2)*180.0f);
}

AttackHelicopterGeometry::Parts AttackHelicopterGeometry::buildParts(Loadout loadout)
{
    // [几何生成] 总装入口：机体比例修正 -> 可动部件转换到铰链原点 -> 挂载/轮胎/尾桨。
    // Fit the earlier stylized airframe's vertical proportions to the reference.
    // Stores, tyres and tail rotor are added afterwards to retain circular sections
    // and the independently documented weapon dimensions.
    constexpr float verticalFit=.72f;
    Parts parts;
    auto& b=parts.body;
    MeshBuilder airframe,mainRotor,stabilator;
    buildAirframe(airframe,mainRotor,stabilator);
    b.appendTransformed(airframe,glm::scale(glm::mat4(1),glm::vec3(1,verticalFit,1)));
    const auto fit=glm::scale(glm::mat4(1),glm::vec3(1,verticalFit,1));
    parts.mainRotor.appendTransformed(mainRotor,pose(-mainRotorPivot())*fit);
    parts.stabilator.appendTransformed(stabilator,pose(-stabilatorPivot())*fit);
    for(int side : {-1,1})
    {
        const float s=static_cast<float>(side);
        hellfireRack(b,{storesX,storesY*verticalFit,s*outboardZ});
        const bool tank=loadout==Loadout::Ferry || (loadout==Loadout::Mixed && side==1);
        const glm::vec3 inner(storesX,(storesY-.015f)*verticalFit,s*inboardZ);
        if(tank) externalTank(b,inner); else rocketPod(b,inner);
        const glm::vec3 wheel(.35f,-.83f*verticalFit,s*.5075f);
        b.addTorus(pose(wheel,90,{1,0,0}),.165f,.055f,64,16,dark);
        b.addSmoothCylinder(pose(wheel,90,{1,0,0}),.125f,.11f,48,armor);
        b.addSmoothCylinder(pose(wheel+glm::vec3(0,0,s*.065f),90,{1,0,0}),.088f,.022f,48,metal);
        b.addTorus(pose(wheel+glm::vec3(0,0,s*.06f),90,{1,0,0}),.12f,.012f,48,10,metal);
        for(int bolt=0;bolt<8;++bolt)
        {
            const float angle=glm::radians(bolt*45.0f);
            b.addSmoothCylinder(pose(wheel+glm::vec3(.064f*std::cos(angle),.064f*std::sin(angle),s*.081f),
                90,{1,0,0}),.009f,.012f,8,dark);
        }
    }
    b.addTorus(pose({-5.08f,-.75f*verticalFit,0},90,{1,0,0}),.12f,.04f,48,12,dark);
    b.addSmoothCylinder(pose({-5.08f,-.75f*verticalFit,0},90,{1,0,0}),.088f,.09f,32,metal);
    const glm::vec3 tailHub(-5.28f,1.15f*verticalFit,-.17f);
    b.addSmoothCylinder(pose(tailHub,90,{1,0,0}),.10f,.27f,12,metal);
    parts.tailBlade.addBox(pose({.34875f,0,0}),{.34875f,.045f,.018f},dark);
    return parts;
}

void AttackHelicopterGeometry::appendTo(MeshBuilder& b, Loadout loadout)
{
    const auto parts=buildParts(loadout);
    b.appendTransformed(parts.body,glm::mat4(1));
    b.appendTransformed(parts.mainRotor,pose(mainRotorPivot()));
    b.appendTransformed(parts.stabilator,pose(stabilatorPivot()));
    for(int i=0;i<4;++i)
        b.appendTransformed(parts.tailBlade,pose(tailRotorPivot(),glm::degrees(tailBladeRestAngle(i)),{0,0,1}));
}

namespace
{
std::shared_ptr<Mesh> uploadPart(const MeshBuilder& builder)
{
    std::vector<glm::vec3> positions,colors,normals;
    std::vector<unsigned int> indices;
    builder.build(positions,colors,normals,indices);
    auto mesh=std::make_shared<Mesh>();
    mesh->upload(positions,colors,normals,indices);
    return mesh;
}
}

AttackHelicopterGeometry::Meshes AttackHelicopterGeometry::createMeshes(Loadout loadout)
{
    // [几何上传] 仅在初始化时执行；尾桨只上传一片，四次绘制时共用同一份 VBO/EBO。
    const auto parts=buildParts(loadout);
    return {uploadPart(parts.body),uploadPart(parts.mainRotor),uploadPart(parts.tailBlade),uploadPart(parts.stabilator)};
}

std::shared_ptr<Mesh> AttackHelicopterGeometry::createMesh(Loadout loadout)
{
    MeshBuilder builder;
    appendTo(builder,loadout);
    std::vector<glm::vec3> positions, colors, normals;
    std::vector<unsigned int> indices;
    builder.build(positions, colors, normals, indices);
    auto mesh = std::make_shared<Mesh>();
    mesh->upload(positions, colors, normals, indices);
    std::cout << "[AttackHelicopter] " << positions.size() << " vertices, " << indices.size()/3 << " triangles\n";
    return mesh;
}

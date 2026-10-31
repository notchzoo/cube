#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include "AttackHelicopterGeometry.h"
#include "AttackHelicopterObject.h"
#include "MeshBuilder.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool finite(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void validate(const MeshBuilder& builder)
{
    std::vector<glm::vec3> p,c,n;
    std::vector<unsigned int> indices;
    builder.build(p,c,n,indices);
    require(!indices.empty() && indices.size()%3==0,"Missing or incomplete triangles");
    require(p.size()==c.size() && p.size()==n.size(),"Vertex attribute count mismatch");
    for(size_t i=0;i<p.size();++i)
    {
        require(finite(p[i]) && finite(n[i]) && finite(c[i]),"Non-finite vertex data");
        require(std::abs(glm::length(n[i])-1.0f)<.001f,"Non-unit normal");
    }
    for(size_t i=0;i<indices.size();i+=3)
    {
        require(indices[i]<p.size() && indices[i+1]<p.size() && indices[i+2]<p.size(),"Invalid index");
        const auto cross=glm::cross(p[indices[i+1]]-p[indices[i]],p[indices[i+2]]-p[indices[i]]);
        require(glm::length(cross)>1e-9f,"Degenerate triangle");
        require(glm::dot(cross,n[indices[i]])>0,"Normal opposes winding");
    }
}

void caps()
{
    MeshBuilder b;
    b.addCylinder(glm::mat4(1),1,2,12,glm::vec3(1));
    std::vector<glm::vec3> p,c,n;
    std::vector<unsigned int> indices;
    b.build(p,c,n,indices);
    int top=0,bottom=0;
    for(size_t i=0;i<indices.size();i+=3)
    {
        const auto a=indices[i],d=indices[i+1],e=indices[i+2];
        if(p[a].y==1 && p[d].y==1 && p[e].y==1) {require(n[a].y>.99f,"Inverted cylinder top cap");++top;}
        if(p[a].y==-1 && p[d].y==-1 && p[e].y==-1) {require(n[a].y<-.99f,"Inverted cylinder bottom cap");++bottom;}
    }
    require(top==12 && bottom==12,"Incomplete cylinder caps");
    MeshBuilder loft;
    loft.addLoft(glm::mat4(1),{{-1,.5f,.5f,0},{1,.5f,.5f,0}},12,glm::vec3(1));
    loft.build(p,c,n,indices);
    int front=0,rear=0;
    for(size_t i=0;i<indices.size();i+=3)
    {
        const auto a=indices[i],d=indices[i+1],e=indices[i+2];
        if(p[a].x==1 && p[d].x==1 && p[e].x==1) {require(n[a].x>.99f,"Inverted loft front cap");++front;}
        if(p[a].x==-1 && p[d].x==-1 && p[e].x==-1) {require(n[a].x<-.99f,"Inverted loft rear cap");++rear;}
    }
    require(front==12 && rear==12,"Incomplete loft caps");
}

void smoothSurfaces()
{
    MeshBuilder cylinder;
    cylinder.addSmoothCylinder(glm::mat4(1),1,2,32,glm::vec3(1));
    validate(cylinder);
    std::vector<glm::vec3> p,c,n;
    std::vector<unsigned int> indices;
    cylinder.build(p,c,n,indices);
    int sides=0,capVertices=0;
    for(size_t i=0;i<p.size();++i)
    {
        if(std::abs(n[i].y)<.5f)
        {
            const auto expected=glm::normalize(glm::vec3(p[i].x,0,p[i].z));
            require(glm::length(n[i]-expected)<.0001f,"Cylinder needs radial vertex normals, not face normals");
            ++sides;
        }
        else
        {
            require(glm::dot(n[i],glm::vec3(0,p[i].y,0))>.99f,"Smooth cylinder cap lost its hard edge");
            ++capVertices;
        }
    }
    require(sides>0 && capVertices>0,"Missing smooth side or flat cap");

    MeshBuilder cone,stretched;
    cone.addSmoothLoft(glm::mat4(1),{{-1,.5f,.5f,0},{1,1,1,0}},32,glm::vec3(1),8);
    const auto transform=glm::scale(glm::mat4(1),glm::vec3(2,.5f,1));
    stretched.appendTransformed(cone,transform);
    validate(stretched);
    stretched.build(p,c,n,indices);
    int curvedVertices=0;
    for(size_t i=0;i<p.size();++i)
    {
        if(std::abs(n[i].x)>.99f) continue; // flat end caps
        const auto local=glm::vec3(p[i].x/2,p[i].y*2,p[i].z);
        const float radius=.75f+.25f*local.x;
        const auto expected=glm::normalize(glm::vec3(-.25f/2,local.y/radius/.5f,local.z/radius));
        require(glm::length(n[i]-expected)<.0002f,"Nonuniform transform flattened or incorrectly transformed smooth normals");
        ++curvedVertices;
    }
    require(curvedVertices>0,"Missing smooth loft sides");

    MeshBuilder torus;
    torus.addTorus(glm::mat4(1),1,.2f,32,12,glm::vec3(1));
    validate(torus);
    torus.build(p,c,n,indices);
    for(size_t i=0;i<p.size();++i)
    {
        const float radius=glm::length(glm::vec2(p[i].x,p[i].z));
        const auto expected=glm::normalize(glm::vec3(p[i].x/radius*(radius-1),p[i].y,p[i].z/radius*(radius-1)));
        require(glm::length(n[i]-expected)<.0002f,"Torus normal is not perpendicular to its tube");
    }
}

void articulation()
{
    using namespace AttackHelicopterGeometry;
    AttackHelicopterObject a({},PBRMaterial{}),b({},PBRMaterial{});
    a.setPedalInput(1);b.setPedalInput(1);
    a.setStabilatorInput(-1);b.setStabilatorInput(-1);
    for(int frame=0;frame<60;++frame) a.update(1.0f/60);
    for(int frame=0;frame<30;++frame) b.update(1.0f/30);
    require(std::abs(a.mainRotorAngle()-b.mainRotorAngle())<.0001f,"Rotor rate depends on frame rate");
    require(std::abs(a.tailRotorAngle()-b.tailRotorAngle())<.0001f,"Tail rotor rate depends on frame rate");
    require(std::abs(a.tailPitchAngle()-b.tailPitchAngle())<.0001f,"Pedal response depends on frame rate");
    require(std::abs(a.stabilatorAngle()-b.stabilatorAngle())<.0001f,"Stabilator response depends on frame rate");
    require(a.tailPitchAngle()>.3f && a.tailPitchAngle()<=glm::radians(18.0f),"Tail pitch not responding or out of bounds");
    require(a.stabilatorAngle()<-.3f && a.stabilatorAngle()>=glm::radians(-20.0f),"Stabilator not responding or out of bounds");

    a.setRotorsEnabled(false);
    const float phase=a.mainRotorAngle(),tailPhase=a.tailRotorAngle();
    a.setPedalInput(0);a.setStabilatorInput(0);a.update(1);
    require(a.mainRotorAngle()==phase && a.tailRotorAngle()==tailPhase,"Paused rotors drift");
    require(std::abs(a.tailPitchAngle())<.001f && std::abs(a.stabilatorAngle())<.001f,"Controls do not return to neutral");
    a.setRotorsEnabled(true);a.update(.01f);
    require(a.mainRotorAngle()!=phase && a.tailRotorAngle()!=tailPhase,"Rotor pause cannot resume");
    const auto root=glm::translate(glm::mat4(1),glm::vec3(4,2,-3))*
        glm::rotate(glm::mat4(1),.7f,glm::vec3(0,1,0))*glm::scale(glm::mat4(1),glm::vec3(.5f));
    a.setModelMatrix(root);a.setRotorAngles(37,83);a.setControlAngles(18,20);
    const auto transforms=a.partTransforms();
    const auto origin=glm::vec4(0,0,0,1);
    require(glm::length(transforms[1]*origin-root*glm::vec4(mainRotorPivot(),1))<.00001f,"Main rotor orbits the wrong pivot");
    for(int i=0;i<4;++i)
        require(glm::length(transforms[i+2]*origin-root*glm::vec4(tailRotorPivot(),1))<.00001f,"Tail blade root moved off the hub");
    require(glm::length(transforms[6]*origin-root*glm::vec4(stabilatorPivot(),1))<.00001f,"Stabilator hinge moves");
    const auto axisPoint=glm::vec4(stabilatorAxis()*.5f,1);
    require(glm::length(transforms[6]*axisPoint-root*glm::vec4(stabilatorPivot()+stabilatorAxis()*.5f,1))<.00001f,
        "Stabilator does not rotate around its spanwise hinge");
    for(int col=0;col<4;++col) require(glm::length(transforms[0][col]-root[col])<.00001f,"Animation moves the fixed airframe");
    a.setRotorAngles(-10,100000);a.update(10000);
    require(a.mainRotorAngle()>=0 && a.mainRotorAngle()<6.283186f && a.tailRotorAngle()>=0 && a.tailRotorAngle()<6.283186f,
        "Rotor angles are not bounded during long runs");
    const float before=a.mainRotorAngle();a.update(std::numeric_limits<float>::quiet_NaN());
    require(a.mainRotorAngle()==before,"Invalid delta time contaminates animation state");
}

void flightIntegration()
{
    const auto root=glm::translate(glm::mat4(1),glm::vec3(-2.8f,1,-1.4f))*
        glm::rotate(glm::mat4(1),glm::radians(-24.0f),glm::vec3(0,1,0))*glm::scale(glm::mat4(1),glm::vec3(.5f));
    AttackHelicopterObject object({},PBRMaterial{},root);
    object.enableFlightPhysics();
    require(glm::length(object.dynamics().state().position-glm::vec3(-11.2f,4,-5.6f))<.0001f,"Render / SI unit conversion changed spawn");
    auto controls=flight::FlightControls{};controls.horizontalVelocity={8,0,0};
    object.setFlightControls(controls);
    for(int i=0;i<180;++i)object.update(1.0f/60);
    require(object.modelMatrix()[3].x>-1.5f,"Physics state is not applied to the render model");
    const auto transforms=object.partTransforms();
    require(glm::length(transforms[1]*glm::vec4(0,0,0,1)-
        object.modelMatrix()*glm::vec4(AttackHelicopterGeometry::mainRotorPivot(),1))<.0001f,"Rotor is detached during whole-aircraft flight");
    const auto position=object.dynamics().state().position;
    object.clearFlightControls();object.enableFlightPhysics();
    require(glm::length(position-object.dynamics().state().position)<.0001f,"Re-entering a vehicle resets its position");

    object.setModelMatrix(glm::rotate(glm::mat4(1),glm::radians(90.0f),glm::vec3(0,1,0)));
    require(glm::length(object.movementDirection(1,0)-glm::vec3(0,0,-1))<.0001f,"Forward movement ignores aircraft heading");
    require(glm::length(object.movementDirection(0,1)-glm::vec3(1,0,0))<.0001f,"Right strafe ignores aircraft heading");
    require(glm::length(object.movementDirection(-1,0)-glm::vec3(0,0,1))<.0001f,"Reverse movement uses the wrong body axis");
    require(std::abs(glm::length(object.movementDirection(1,1))-1)<.0001f,"Body-relative diagonals are faster");
    object.setModelMatrix(glm::rotate(glm::mat4(1),glm::radians(35.0f),glm::vec3(0,0,1)));
    require(glm::length(object.movementDirection(1,0)-glm::vec3(1,0,0))<.0001f,"Aircraft pitch leaks into horizontal movement");
}

void proximityInteraction()
{
    const auto model=glm::translate(glm::mat4(1),glm::vec3(-2.8f,1,-1.4f))*
        glm::rotate(glm::mat4(1),glm::radians(-24.0f),glm::vec3(0,1,0))*glm::scale(glm::mat4(1),glm::vec3(.5f));
    AttackHelicopterObject object({},PBRMaterial{},model);
    const auto point=object.interactionPoint();
    const auto radius=object.interactionRadius();
    require(object.canEnterFrom(point+glm::vec3(radius*.99f,0,0)),"Nearby cockpit interaction is rejected");
    require(!object.canEnterFrom(point+glm::vec3(radius*1.01f,0,0)),"Distant cockpit interaction is accepted");
    require(!object.canEnterFrom(point+glm::vec3(0,radius*1.01f,0)),"Interaction ignores vertical separation");
    require(!object.canEnterFrom(glm::vec3(0,3,6.6f)),"Default distant camera can board remotely");
    require(!object.canEnterFrom(glm::vec3(model*glm::vec4(-5.28f,1,0,1))),"Tail rotor can be used as a cockpit entry");
    object.setModelMatrix(glm::translate(glm::mat4(1),glm::vec3(0,20,0))*model);
    require(!object.canEnterFrom(point),"Boarding target does not follow the moving aircraft");
    require(object.canEnterFrom(object.interactionPoint()),"Moved aircraft cannot be boarded nearby");
}
}

int main()
{
    try
    {
        caps();
        smoothSurfaces();
        articulation();
        flightIntegration();
        proximityInteraction();
        using namespace AttackHelicopterGeometry;
        for(auto loadout : {Loadout::Mixed,Loadout::Combat,Loadout::Ferry})
        {
            MeshBuilder builder;
            appendTo(builder,loadout);
            validate(builder);
        }
        const auto parts=buildParts();
        validate(parts.body);validate(parts.mainRotor);validate(parts.tailBlade);validate(parts.stabilator);
        std::cout<<"AH-64D: geometry, articulated parts, hinges, frame rates, rotor pause and control return verified\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}

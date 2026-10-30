#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vColor;
// Object space position and normal: the dice pips are painted in face-local
// coordinates, which stay unit-sized no matter how the model matrix places,
// rotates or scales the cube.
out vec3 vLocalPosition;
out vec3 vLocalNormal;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;

    // This project only uses uniform scale, but the inverse-transpose keeps
    // normals correct if a non-uniform model matrix is introduced later.
    vWorldNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vColor = aColor;
    vLocalPosition = aPosition;
    vLocalNormal = aNormal;

    gl_Position = uProjection * uView * worldPosition;
}
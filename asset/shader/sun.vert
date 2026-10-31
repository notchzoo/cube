#version 330 core

layout (location = 0) in vec2 aCorner;   // billboard 本地坐标，范围 [-1, 1]

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uSunCenter;
uniform vec3 uSunRight;
uniform vec3 uSunUp;
uniform float uHalfSize;

out vec2 vLocal;

void main()
{
    vLocal = aCorner;
    vec3 worldPosition = uSunCenter + (uSunRight * aCorner.x + uSunUp * aCorner.y) * uHalfSize;
    gl_Position = uProjection * uView * vec4(worldPosition, 1.0);
}
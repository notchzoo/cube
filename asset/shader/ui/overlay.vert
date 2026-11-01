#version 330 core

layout(location = 0) in vec2 aPosition;  // 像素坐标，左上原点
layout(location = 1) in vec2 aLocal;     // 相对图元中心的偏移（SDF 用）
layout(location = 2) in vec2 aHalfSize;  // 图元半尺寸
layout(location = 3) in vec2 aUv;        // 字形图集坐标
layout(location = 4) in vec4 aColor;
layout(location = 5) in float aRadius;   // 圆角半径
layout(location = 6) in float aKind;     // 0 = 实心圆角矩形，1 = 字形

uniform vec2 uViewport;

out vec2 vLocal;
out vec2 vHalfSize;
out vec2 vUv;
out vec4 vColor;
out float vRadius;
out float vKind;

void main()
{
    vLocal = aLocal;
    vHalfSize = aHalfSize;
    vUv = aUv;
    vColor = aColor;
    vRadius = aRadius;
    vKind = aKind;

    vec2 ndc = vec2(aPosition.x / uViewport.x * 2.0 - 1.0, 1.0 - aPosition.y / uViewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}

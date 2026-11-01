#version 330 core

in vec2 vLocal;
in vec2 vHalfSize;
in vec2 vUv;
in vec4 vColor;
in float vRadius;
in float vKind;

uniform sampler2D uAtlas;

out vec4 fragColor;

float roundedBoxSdf(vec2 point, vec2 halfSize, float radius)
{
    vec2 offset = abs(point) - halfSize + radius;
    return min(max(offset.x, offset.y), 0.0) + length(max(offset, 0.0)) - radius;
}

void main()
{
    if (vKind < 0.5)
    {
        // 圆角矩形用 SDF 做一像素级抗锯齿：面板边缘不依赖 MSAA，离屏截图也一致。
        float distance = roundedBoxSdf(vLocal, vHalfSize, max(vRadius, 0.0));
        float alpha = 1.0 - smoothstep(-1.0, 0.5, distance);
        fragColor = vec4(vColor.rgb, vColor.a * alpha);
    }
    else
    {
        float coverage = texture(uAtlas, vUv).r;
        fragColor = vec4(vColor.rgb, vColor.a * coverage);
    }
}

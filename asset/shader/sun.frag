#version 330 core

in vec2 vLocal;

out vec4 fragColor;

uniform vec3 uTint;
uniform float uRadiance;
uniform float uCoreRatio;      // 日盘半径 / 光晕半径
uniform int uRayCount;
uniform float uRaySharpness;
uniform float uRayStrength;
uniform int uUseTexture;
uniform sampler2D uSunTexture;

// 场景的色调映射是在各个着色器里做的（不是后处理），这里保持一致。
vec3 tonemap(vec3 hdr)
{
    hdr = hdr / (hdr + vec3(1.0));
    return pow(hdr, vec3(1.0 / 2.2));
}

void main()
{
    float radial = length(vLocal);
    if (radial > 1.0)
    {
        discard;
    }

    vec3 color;
    float alpha;

    if (uUseTexture != 0)
    {
        // 贴图模式：约定"亮度当 alpha"，白不透明、黑透明。这样任意画好的 BMP 都能直接用。
        vec2 uv = vLocal * 0.5 + 0.5;
        vec3 texel = texture(uSunTexture, uv).rgb;
        float luma = dot(texel, vec3(0.2126, 0.7152, 0.0722));
        float falloff = 1.0 - smoothstep(0.80, 1.0, radial);
        color = texel;
        alpha = clamp(luma * falloff, 0.0, 1.0);
    }
    else
    {
        float core = 1.0 - smoothstep(uCoreRatio * 0.70, uCoreRatio, radial);
        float glow = pow(max(0.0, 1.0 - radial), 2.6);
        float bloom = pow(max(0.0, 1.0 - radial), 7.0);

        // 两组不同数量的光芒叠加，避免规则星形；宽度随距离衰减。
        float angle = atan(vLocal.y, vLocal.x);
        float raysA = pow(max(0.0, 0.5 + 0.5 * cos(angle * float(uRayCount))), uRaySharpness);
        float raysB = pow(max(0.0, 0.5 + 0.5 * cos(angle * float(uRayCount) * 0.55 + 1.7)),
                          uRaySharpness * 1.8);
        float rays = (raysA + raysB * 0.6) * pow(max(0.0, 1.0 - radial), 1.6) * uRayStrength;

        float intensity = core * uRadiance + glow * 2.0 + bloom * 3.2 + rays * 2.6;
        color = tonemap(uTint * intensity);
        alpha = clamp(core + glow * 0.80 + bloom * 0.55 + rays, 0.0, 1.0);
    }

    fragColor = vec4(color, alpha);
}
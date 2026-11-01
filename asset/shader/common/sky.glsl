// 程序化天空（线性 HDR）。
//
// pbr.frag 用它算环境漫反射、环境镜面反射和距离雾；skyDome.frag 用它画背景。
// 这份实现是唯一来源，由 Shader::loadFromFiles 的 #include 预处理展开，
// 因此“骰子的反射与背景颜色一致”不再依赖两处人手同步。
#ifndef PROCEDURAL_SKY_INCLUDED
#define PROCEDURAL_SKY_INCLUDED

vec3 proceduralSky(vec3 direction)
{
    float height = clamp(direction.y, -1.0, 1.0);

    vec3 zenithColor  = vec3(0.07, 0.19, 0.48);
    vec3 horizonColor = vec3(0.55, 0.66, 0.82);
    vec3 groundColor  = vec3(0.20, 0.20, 0.22);

    vec3 sky = mix(horizonColor, zenithColor, pow(clamp(height, 0.0, 1.0), 0.55));
    return mix(groundColor, sky, smoothstep(-0.05, 0.05, height));
}

#endif

#version 330 core

in vec3 vDirection;

out vec4 fragColor;

// 背景天空与 pbr.frag 的环境反射共用同一份实现（asset/shader/common/sky.glsl），
// 由 Shader 的 #include 预处理展开，不再靠两处人手同步。
#include "common/sky.glsl"

void main()
{
    vec3 color = proceduralSky(normalize(vDirection));

    // Same tone mapping and gamma as the PBR pass so the sky and the lit
    // objects live in one consistent display space.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    fragColor = vec4(color, 1.0);
}

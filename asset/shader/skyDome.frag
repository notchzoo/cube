#version 330 core

in vec3 vDirection;

out vec4 fragColor;

// Keep numerically identical to proceduralSky() in pbr.frag.
vec3 proceduralSky(vec3 direction)
{
    float height = clamp(direction.y, -1.0, 1.0);

    vec3 zenithColor  = vec3(0.07, 0.19, 0.48);
    vec3 horizonColor = vec3(0.55, 0.66, 0.82);
    vec3 groundColor  = vec3(0.20, 0.20, 0.22);

    vec3 sky = mix(horizonColor, zenithColor, pow(clamp(height, 0.0, 1.0), 0.55));
    return mix(groundColor, sky, smoothstep(-0.05, 0.05, height));
}

void main()
{
    vec3 color = proceduralSky(normalize(vDirection));

    // Same tone mapping and gamma as the PBR pass so the sky and the lit
    // objects live in one consistent display space.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    fragColor = vec4(color, 1.0);
}
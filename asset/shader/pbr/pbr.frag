#version 330 core

const float PI = 3.14159265359;

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vColor;
in vec3 vLocalPosition;
in vec3 vLocalNormal;

out vec4 fragColor;

uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;   // travel direction of the light, normalized
uniform vec3 uLightRadiance;

uniform vec3 uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform float uAo;
uniform float uOpacity;

uniform sampler2D uShadowMap;
uniform mat4 uLightSpaceMatrix;
uniform int uUseShadowMap;

uniform float uReflectionStrength;

// Opacity of the shadow caster (the cube), not of the fragment being shaded.
// It scales how much of the cast shadow stays visible on every receiver.
uniform float uShadowStrength;

// Dice pips painted on the current face: 0 = plain surface, 1..6 = pip count.
uniform int uPipCount;

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denominator = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 1e-7);
}

float geometrySchlickGGX(float NdotX, float roughness)
{
    // Direct lighting: k = (r + 1)^2 / 8
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-7);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Keep numerically identical to proceduralSky() in skyDome.frag: the
// environment reflection reuses the very same sky function, so a cube
// reflection always matches the background colors.
vec3 proceduralSky(vec3 direction)
{
    float height = clamp(direction.y, -1.0, 1.0);

    vec3 zenithColor  = vec3(0.07, 0.19, 0.48);
    vec3 horizonColor = vec3(0.55, 0.66, 0.82);
    vec3 groundColor  = vec3(0.20, 0.20, 0.22);

    vec3 sky = mix(horizonColor, zenithColor, pow(clamp(height, 0.0, 1.0), 0.55));
    return mix(groundColor, sky, smoothstep(-0.05, 0.05, height));
}

// Painted dice pips, evaluated in object space. The cube is a unit cube, so a
// face spans [-1, 1] and a 3x3 grid of pip cells covers 1..6 pips. This is a
// shading trick, not geometry: the pips have no relief and cast no own shadow.
float dicePipMask(vec3 localPosition, vec3 localNormal, int pipCount, float pixelSize)
{
    if (pipCount <= 0)
    {
        return 0.0;
    }

    // Object space axes that lie in the face plane. Using the object space normal
    // keeps the layout correct even when the model matrix rotates the cube.
    vec2 faceCoordinates;
    if (abs(localNormal.x) > 0.5)
    {
        faceCoordinates = localPosition.zy;
    }
    else if (abs(localNormal.y) > 0.5)
    {
        faceCoordinates = localPosition.xz;
    }
    else
    {
        faceCoordinates = localPosition.xy;
    }

    // Bit i enables grid cell i, where index = column + row * 3.
    int mask = 0;
    if (pipCount == 1)      mask = 16;                 // centre
    else if (pipCount == 2) mask = 257;                // opposite corners
    else if (pipCount == 3) mask = 273;                // diagonal
    else if (pipCount == 4) mask = 325;                // four corners
    else if (pipCount == 5) mask = 341;                // four corners + centre
    else                    mask = 365;                // two columns of three

    const float pipRadius = 0.15;
    const float gridOffset = 0.57;
    float edge = max(pixelSize * 1.5, 0.002);
    float coverage = 0.0;

    for (int index = 0; index < 9; ++index)
    {
        if ((mask & (1 << index)) == 0)
        {
            continue;
        }

        float column = float(index % 3) - 1.0;
        float row = float(index / 3) - 1.0;
        vec2 center = vec2(column, row) * gridOffset;
        float distanceToCenter = length(faceCoordinates - center);
        coverage = max(coverage, 1.0 - smoothstep(pipRadius - edge, pipRadius + edge, distanceToCenter));
    }

    return coverage;
}

// 3x3 PCF comparison in light space. Border samples are white (far), so
// anything outside the light frustum counts as fully lit.
float shadowVisibility(vec3 N, vec3 L)
{
    if (uUseShadowMap == 0)
    {
        return 1.0;
    }

    vec4 lightSpacePosition = uLightSpaceMatrix * vec4(vWorldPosition, 1.0);
    vec3 projected = lightSpacePosition.xyz / max(lightSpacePosition.w, 1e-5);
    projected = projected * 0.5 + 0.5;

    if (projected.z > 1.0 || projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0)
    {
        return 1.0;
    }

    // Slope dependent bias: grazing angles need a larger tolerance.
    float bias = max(0.0022 * (1.0 - dot(N, L)), 0.0006);
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));

    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(uShadowMap, projected.xy + vec2(float(x), float(y)) * texelSize).r;
            visibility += (projected.z - bias) > closestDepth ? 0.0 : 1.0;
        }
    }

    return visibility / 9.0;
}

void main()
{
    // Vertex color tints the material albedo, which keeps the Mesh color
    // attribute meaningful (both are white by default in this scene).
    vec3 albedo = uAlbedo * vColor;
    float metallic = uMetallic;
    float roughness = uRoughness;

    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uCameraPosition - vWorldPosition);
    vec3 L = normalize(-uLightDirection);
    vec3 H = normalize(V + L);

    // Painted dice pips modulate the albedo before F0 is derived, so a metallic
    // face gets dark metal pips instead of a flat black decal.
    float pixelSize = max(fwidth(vLocalPosition.x),
                          max(fwidth(vLocalPosition.y), fwidth(vLocalPosition.z)));
    float pipCoverage = dicePipMask(vLocalPosition, vLocalNormal, uPipCount, pixelSize);
    albedo = mix(albedo, vec3(0.015), pipCoverage * 0.95);

    float NdotV = max(dot(N, V), 1e-4);
    float NdotL = max(dot(N, L), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (NDF * G * F) / max(4.0 * NdotV * NdotL, 1e-4);

    // Energy conservation: light reflected specularly cannot also be diffuse,
    // and metals have no diffuse lobe at all.
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    // The depth pass writes the transparent cube, so its shadow exists; the
    // caster's opacity decides how dark that shadow appears on every receiver
    // (a 60% cube blocks about 60% of the direct light).
    float occlusion = shadowVisibility(N, L);
    float shadowTerm = 1.0 - (1.0 - occlusion) * uShadowStrength;

    vec3 direct = (kD * albedo / PI + specular) * uLightRadiance * NdotL * shadowTerm;

    // No HDR IBL in this project: the same procedural sky approximates both
    // ambient irradiance and specular reflection. This is an approximation,
    // not prefiltered image based lighting.
    vec3 irradiance = mix(proceduralSky(vec3(0.0, -1.0, 0.0)),
                          proceduralSky(vec3(0.0, 1.0, 0.0)),
                          N.y * 0.5 + 0.5);
    vec3 ambientDiffuse = kD * albedo * uAo * irradiance * 0.30;

    vec3 reflectionDirection = reflect(-V, N);
    vec3 environmentSpecular = proceduralSky(reflectionDirection) *
                               fresnelSchlickRoughness(NdotV, F0, roughness) *
                               uReflectionStrength * uAo;

    vec3 color = direct + ambientDiffuse + environmentSpecular;

    // Distance fog toward the horizon color (linear HDR, before tone mapping).
    // It hides the far edge of the ground plane and gives the flat ground
    // some atmospheric depth.
    float cameraDistance = length(uCameraPosition - vWorldPosition);
    float fogFactor = 1.0 - exp(-cameraDistance * 0.0022);
    vec3 horizonDirection = normalize(vec3(-V.x, 0.0, -V.z));
    color = mix(color, proceduralSky(horizonDirection), fogFactor);

    // Reinhard tone mapping, then Gamma correction.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    // Alpha blending models transmission only; there is no refraction here.
    fragColor = vec4(color, uOpacity);
}
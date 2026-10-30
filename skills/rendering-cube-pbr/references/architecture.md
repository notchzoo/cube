# Architecture and ownership

Use this layout as the default. Adapt names when an existing project already has a coherent convention.

```text
project/
├─ application/
│  ├─ Application.h
│  └─ Application.cpp
├─ glframework/
│  ├─ Shader.h/.cpp
│  ├─ Camera.h/.cpp
│  ├─ ShadowMap.h/.cpp
│  ├─ SkyDome.h/.cpp
│  ├─ geometry/
│  │  └─ Mesh.h/.cpp
│  └─ lighting/
│     ├─ DirectionalLight.h
│     ├─ PBRMaterial.h
│     └─ PBRRenderer.h/.cpp
├─ asset/shader/
│  ├─ pbr/pbr.vert
│  ├─ pbr/pbr.frag
│  ├─ shadowDepth.vert/.frag
│  └─ skyDome.vert/.frag
├─ src/main.cpp
└─ CMakeLists.txt
```

## Component boundaries

### Application

Own the GLFW lifecycle, callbacks, input polling, scene objects, render-pass order, and public configuration API. Keep raw OpenGL buffer creation out of this class.

Recommended public API:

```cpp
void setDirectionalLight(const DirectionalLight& light);
void setLightDirection(const glm::vec3& direction);
void setCubeMaterial(const PBRMaterial& material);
void setGroundMaterial(const PBRMaterial& material);
void setReflectionStrength(float strength);
void setObjectOpacity(float opacity);
void enableShadows(bool enabled);
```

### Mesh

Own one VAO, position/color/normal VBOs, one EBO, and the index count. Use fixed locations:

- `0`: position
- `1`: vertex color
- `2`: normal

`draw()` must bind the VAO and call `glDrawElements`. `reset()` must be safe to call repeatedly.

### PBRRenderer

Own the PBR Shader and all PBR uniform naming. `beginFrame()` sets camera, matrices, direction-light radiance, shadow settings, and environment strength. `draw()` sets one material and model matrix, then draws one Mesh.

### ShadowMap

Own the depth texture and depth-only framebuffer. Default to 2048×2048, clamp outside samples to a white border, and use a 3×3 PCF comparison in the PBR fragment shader.

### SkyDome

Generate an inward-viewed UV sphere. Remove translation from the View matrix, disable depth writes while drawing, and place the sphere at far depth with `xyww`. Keep the procedural sky function numerically consistent with the environment-reflection approximation in the PBR shader.

## Render order

```text
1. Shadow pass
   ├─ bind shadow framebuffer
   ├─ render ground depth
   └─ render cube depth

2. Main pass
   ├─ restore window viewport
   ├─ clear color and depth
   ├─ draw sky dome
   ├─ draw opaque ground with PBR
   └─ draw cube with PBR (transparent last when alpha < 1)
```

## PBR material defaults

Use a dielectric cube and rough ground as safe defaults:

```cpp
PBRMaterial cube { glm::vec3(1.0f), 0.0f, 0.28f, 1.0f, 1.0f };
PBRMaterial ground { glm::vec3(1.0f), 0.0f, 0.90f, 1.0f, 1.0f };
```

High ground roughness prevents the broad clipped white highlight produced when a Phong material is shared with the cube.

## Resource lifetime

Call these while the context is current, before `glfwDestroyWindow`:

```text
SkyDome::reset
ShadowMap::reset
Shadow Shader::reset
PBRRenderer::reset
ground Mesh::reset
cube Mesh::reset
```

Destructors may call `reset()` again, so reset functions must zero handles after deletion.

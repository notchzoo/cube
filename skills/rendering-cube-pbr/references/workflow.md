# Complete creation workflow

This workflow recreates the project from an empty directory while keeping every stage buildable.

## 1. Inventory dependencies

Confirm the actual paths before writing CMake:

```text
GLFW include/GLFW/glfw3.h
GLFW lib/glfw3.lib
GLAD include/glad/glad.h
GLAD include/KHR/khrplatform.h
GLAD src/glad.c
GLM glm/glm.hpp
```

Prefer user-provided GLFW and GLAD. Keep GLM under `third_party/glm` when the workspace has no shared copy. Do not silently replace library versions.

Exit criterion: dependency files exist and their architecture matches the planned x64 build.

## 2. Create the CMake skeleton

Create `application`, `glframework`, `glframework/geometry`, `glframework/lighting`, `asset/shader/pbr`, and `src`.

CMake requirements:

- C and C++ languages, C++17.
- Compile GLAD as a static target.
- Import the supplied GLFW static library and include directory.
- Link `opengl32`, `user32`, `gdi32`, and `shell32` on Windows.
- Add `/utf-8` for Chinese source comments under MSVC.
- Define an absolute `RENDERINGCUBE_ASSET_DIR` for runtime Shader loading.
- Use `source_group(TREE ...)` so Visual Studio mirrors real folders.

Exit criterion: an empty executable configures and links.

## 3. Build the window lifecycle

Implement `Application` with:

1. GLFW error callback and initialization.
2. OpenGL 3.3 Core hints and a resizable window.
3. Current context followed by `gladLoadGLLoader`.
4. Framebuffer resize callback calling `glViewport`.
5. Main loop: process input, render, swap, poll.
6. Shutdown that releases GL resources before GLFW termination.

Use a default client size chosen by the user. Do not assume the framebuffer size equals the window size on HiDPI displays.

Exit criterion: a clear-color window opens, resizes, and closes cleanly.

## 4. Add Shader and Camera

Shader must load text files, compile stages, link programs, print driver logs, and provide `setMat4`, `setVec3`, `setFloat`, and `setInt`.

Camera must expose a View matrix and position. Use frame-time-scaled movement:

- W/S: forward/backward
- A/D: left/right
- Page Up/Page Down: world up/down
- mouse wheel: world-Y elevation in fixed increments
- mouse delta: yaw/pitch, with pitch clamped to ±89°

For locked-camera input, use `GLFW_CURSOR_DISABLED`; enable raw mouse motion when supported.

Exit criterion: the camera moves consistently at different frame rates.

## 5. Add Mesh and scene geometry

Implement `Mesh` before adding scene geometry. Upload position, color, and normal arrays into separate VBOs and indices into the EBO.

Create the cube with 24 vertices and 36 indices. Each face needs four unique vertices and one constant face normal. Create a large four-vertex ground plane below the cube.

Exit criterion: cube and ground draw through `Mesh::draw`; Application owns no raw geometry handles.

## 6. Add direct-light PBR

Create `DirectionalLight`, `PBRMaterial`, and `PBRRenderer`. Use the metallic/roughness workflow:

```text
albedo, metallic, roughness, ao, opacity
```

The fragment shader must implement:

1. GGX/Trowbridge-Reitz normal distribution.
2. Schlick-GGX plus Smith geometry term.
3. Schlick Fresnel with `F0 = mix(0.04, albedo, metallic)`.
4. Energy conservation: `kD = (1 - kS) * (1 - metallic)`.
5. Directional-light radiance.
6. Reinhard tone mapping followed by `pow(color, 1/2.2)` Gamma correction.

Clamp roughness to `[0.04, 1]`. Use a rough ground material around `0.9` to avoid an artificial broad highlight.

Exit criterion: varying metallic and roughness produces visibly different but stable highlights.

## 7. Add directional shadow mapping

Create a depth-only framebuffer and depth texture. For the directional light, construct a virtual light camera with orthographic projection; this does not give the light a physical position.

Render two passes:

1. Depth pass from light space.
2. PBR pass sampling the depth texture.

Use slope-dependent bias, polygon offset, white border depth, and 3×3 PCF. Restore the window framebuffer viewport after the shadow pass.

Exit criterion: the cube casts a stable shadow on the ground without strong acne or edge aliasing.

## 8. Add the procedural sky dome

Generate a UV sphere in code. Draw it before scene geometry, with View translation removed and depth writes disabled. Use a simple zenith/horizon/ground gradient.

For lightweight environment reflection without HDR IBL, sample the same procedural sky function using the reflection direction and combine it with roughness-aware Fresnel. Document that this is an approximation, not prefiltered image-based lighting.

Exit criterion: the background follows camera rotation but not camera translation, and object reflection matches the sky colors.

## 9. Add transparency safely

Enable standard alpha blending. Draw sky first, opaque ground next, and the potentially transparent cube last. Keep depth testing enabled; disable depth writes only during the transparent cube draw.

The shadow depth pass must still write cube depth. Scale the shadow contribution by cube opacity in the PBR pass. State clearly that alpha blending is transmission without physical refraction.

Exit criterion: the background is visible through alpha below one, alpha one is opaque, and alpha zero does not hide the ground through depth writes.

## 10. Expose Application configuration

Keep main short. Offer setters for light direction/full light, cube and ground materials, environment strength, opacity, and shadow enablement. Clamp unsafe scalar inputs.

Exit criterion: common visual changes require no edits inside PBRRenderer or GLSL.

## 11. Verify and hand off

Run:

```powershell
./.agents/skills/rendering-cube-pbr/scripts/verify_project.ps1 -ProjectRoot .
```

Then launch the Debug executable for several seconds. Check stderr for Shader compile/link or framebuffer errors. Visually check:

- PBR roughness changes highlight width.
- Ground has no clipped white disk.
- Shadow follows light direction.
- Sky does not translate with the camera.
- Transparent cube blends over the already drawn sky and ground.
- Closing the app produces no GL calls after context destruction.

---
name: rendering-cube-pbr
description: Create or refactor a maintainable C++ OpenGL 3.3 project using GLFW, GLAD, CMake, camera controls, Mesh ownership, Cook-Torrance PBR, directional-light shadow mapping, and a procedural sky dome. Use for projects based on this RenderingCube architecture; do not use for Vulkan, DirectX, or hardware ray tracing.
---

# Rendering Cube PBR

Build a working project, not an isolated code sample. Preserve user-provided dependency locations and existing behavior unless the request changes them.

## Route the work

- For a new project or a complete rebuild, read [references/workflow.md](references/workflow.md) and follow its phases in order.
- For refactoring, adding rendering features, or deciding file ownership, read [references/architecture.md](references/architecture.md).
- For a narrow fix, inspect the current implementation first and load only the relevant reference sections.

## Required invariants

- Create the OpenGL context before any GL object; delete GL objects before destroying the context.
- Include GLAD before GLFW and define `GLFW_INCLUDE_NONE`.
- Keep window/input orchestration in `Application`, buffer ownership in `Mesh`, and PBR uniform setup in `PBRRenderer`.
- Use 24 vertices for a hard-edged cube so each face owns a flat normal.
- Draw the procedural sky before transparent objects. Draw opaque geometry before transparent geometry; keep depth testing enabled and disable depth writes only while drawing transparent geometry.
- In the shadow pass, transparent objects must still write depth. Scale their visible shadow contribution in the lit pass instead.
- Treat a directional light as direction plus radiance. A virtual light camera position exists only to construct its shadow matrix.
- Clamp PBR roughness to at least `0.04`, apply HDR tone mapping, then Gamma correction.
- OpenGL 3.3 GLSL has no native `#include`; keep required BRDF helpers in the shader unless the project adds a shader preprocessor.

## Completion standard

Regenerate CMake, perform a clean Debug build, and run the executable long enough to compile runtime GLSL and initialize framebuffer resources. Treat a successful C++ build as insufficient when shaders changed. Keep comments focused on ownership, coordinate spaces, render ordering, and non-obvious equations.

Use [scripts/verify_project.ps1](scripts/verify_project.ps1) for the build portion of verification.

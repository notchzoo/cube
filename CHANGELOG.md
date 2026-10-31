# 版本记录

## v0.11 — 2026-10-31

把工作区里新增的「太阳图层」整合为独立版本 v0.11：新增可选太阳模块、太阳贴图材质、
相机摆位开关，并补上完整使用手册与太阳截图。

### 新增

- **太阳图层（SunSprite，可选模块）**
  - `glframework/SunSprite.h` / `glframework/SunSprite.cpp`
  - `asset/shader/sun.vert` / `asset/shader/sun.frag`
  - 位置由光方向决定（`sunDir = -light.direction`），太阳、阴影、骰子高光天然自洽，不引入额外的“太阳位置”状态。
  - 只依赖 GL 3.3 core 与工程已有的 `Shader` 类：不改 `SkyDome` / `PBRRenderer` / 任何现有着色器；
    删掉 `Application` 里的 `m_sun.render(...)` 一行即等于整体删除太阳；`sun.vert|frag` 缺失只禁用太阳，不影响启动。
  - 外观可换：`SunStyle{ tint, radiance, coreDegrees, glowDegrees, rayCount, raySharpness, rayStrength }`，
    或用 `setSunTexture("xxx.bmp")` 换贴图材质（约定“亮度当 alpha”，白不透明、黑透明，支持未压缩 24/32 位 BMP）。
- **示例太阳贴图**：`asset/texture/sun_soft.bmp`（`.gitignore` 加了 `!asset/texture/*.bmp` 例外，其余 `*.bmp` 仍不入库）。
- **使用手册**：`docs/MANUAL.md`，349 行，覆盖构建、目录职责、渲染流水线、参数、已知近似与太阳图层。
- **太阳截图**：`docs/renderingcube-sun.png`（程序化）、`docs/renderingcube-sun-texture.png`（贴图）。

### 接口变更（`application/Application.h`）

```cpp
void setSunStyle(const SunStyle& style);
const SunStyle& sunStyle() const;
bool setSunTexture(const std::string& path);
void clearSunTexture();
void setCameraPose(const glm::vec3& position, float yawDegrees, float pitchDegrees); // 截图/调试用
```

渲染顺序：天空球之后、不透明几何之前绘制太阳；太阳不写深度、不参与 PBR 与天空计算。

### 命令行新增（`src/main.cpp`）

| 参数 | 作用 |
| --- | --- |
| `--sun <radiance>` | 改太阳亮度（默认 26） |
| `--sun-texture <path.bmp>` | 换成贴图材质 |
| `--camera x y z yaw pitch` | 直接摆相机（yaw/pitch 单位为度），便于把太阳放进画面 |

### 构建与文档

- `CMakeLists.txt` 纳入 `SunSprite.*` 与 `sun.vert|frag`。
- `README.md` 新增「太阳图层（SunSprite）」章节与版本标记。
- 验证：`skills/rendering-cube-pbr/scripts/verify_project.ps1 -ProjectRoot .` 干净构建通过（Debug）。

### 已知取舍

- 太阳没有写进 `proceduralSky()`，所以骰子的环境反射里看不到它——这是「低耦合」的代价；
  要让它进入反射必须改 `pbr.frag`，会破坏零依赖。

## 初始版本 — 2026-10-30（`64bcd70`）

基于 GLFW + GLAD + GLM 的 OpenGL 3.3 Core 渲染框架：硬边立方体 + 超大地面片，
Cook-Torrance PBR（GGX / Smith / Schlick Fresnel）、方向光阴影贴图（3x3 PCF）、
程序化天空球与粗糙度感知的环境反射近似，最后 Reinhard 色调映射 + Gamma 校正。
# 版本记录

## v0.12.01 — 2026-11-02

本轮是“工程质量”版本：不新增画面内容，集中修掉性能热点、构建/发布限制与开发期反馈回路。

### P0 渲染正确性与性能

- uniform 位置改为首次查询后缓存（`Shader::uniformLocation`），每帧不再调用上百次
  `glGetUniformLocation`；Debug 下拼错的名字会打印一次诊断。
- 着色器加载支持 `#include "相对路径"`（相对当前文件解析、单阶段去重、深度上限 8）。
  新增 `asset/shader/common/sky.glsl`，`pbr.frag` 与 `skyDome.frag` 改为包含同一份实现，
  删掉了两处人手同步的 `proceduralSky()`。
- 离屏截图改为“多重采样 renderbuffer 渲染 + blit 解析”：`--screenshot` 与窗口一致使用 4x MSAA，
  基准图从此带抗锯齿（输出行也会标注采样数）。
- 新增 `glframework/GlUtil.h`：move-only 的 Texture/Framebuffer/Renderbuffer 句柄，
  截图 FBO 与覆盖层缓冲析构即释放；`gl::checkError()` 在 Debug 下把各 pass 的 GL 错误
  变成带位置的日志行。新增 `tools/compare_screenshot.py` 用于逐像素回归比较。

### P1 开发期反馈与可移植性

- 新增 `glframework/ui/Overlay.*`（即时模式覆盖层），彻底移除 Win32 GDI 速度面板与
  `Speedometer`：文字在启动时由系统字体一次性栅格化成图集，每帧只做纹理采样；
  圆角面板用 SDF 抗锯齿；GL 状态由 `GlStateBlock` 一次性保存/恢复。
- F1 打开调参面板（`--ui` 可在启动时打开）：帧率/帧时间、绘制项与三角形数、
  阴影、反射强度、不透明度、粗糙度、金属度、点数、自转、光照方位与高度、旋翼。
  面板可见时释放光标；关闭后恢复原来的鼠标观察状态。
- 新增 `ShaderWatcher`：每 0.4 秒检查着色器及其 `#include` 依赖的 mtime 并热重载；
  链接失败保留旧程序，只打印驱动日志。
- 命令行改为自检式：新增 `--help/-h`、`--version`、`--ui`；未知选项与数字解析失败
  一律报错并以退出码 2 结束，不再静默使用默认值。
- 资产目录改为运行期解析（exe 同级 `asset/` 优先，源码树路径兜底），
  构建后 CMake 会把 `asset/` 复制到输出目录，产物可以整体拷走运行。
- 工程化：新增 `.clang-format`（Allman + 4 空格 + 120 列）并对全部源码格式化；
  新增 `CMakePresets.json`（debug/release）、`.github/workflows/build.yml`
  （configure → build → ctest）、`RENDERINGCUBE_VERSION` 宏；
  修正 `.gitignore` 的中文注释乱码并新增 `/verify_out/`。

### 验证

- `cmake --preset debug` + `cmake --build --preset debug` + `ctest --preset debug` 全绿。
- 格式化、uniform 缓存、`#include` 重构三步各自用 `--screenshot` 与基线逐像素对比（0 差异）；
  MSAA 截图与基线差 0.785% 像素、最大通道差 103，全部位于几何边缘，符合预期。
- 热重载成功与失败两条路径、`--help/--version/未知参数`、`--drive` 进入驾驶、`--ui` 面板均实测通过。

### 已知遗留

- `third_party/glfw/lib/glfw3.lib` 仍是 Debug CRT 构建，因此所有配置继续使用 `/MDd`；
  真正可分发的 Release 需要一份同源的 Release CRT 版静态库。
- `docs/` 里部分旧截图的 HUD 形态仍是旧的速度面板，画面内容（PBR/阴影/载具）未变。
## v0.12 — 2026-10-31

本轮包含场景对象化重构、AH-64D 外形建模与细化、部件动画、整机飞行动力学、
第三人称驾驶、近距离交互和右上角实时速度显示。

### AH-64D 模型与曲面细化

- 新增 `glframework/geometry/AttackHelicopterGeometry.*` 和 `glframework/scene/AttackHelicopterObject.*`，
  默认场景移除原红色直升机，改为 AH-64D 风格武装直升机；旧模型源码保留。
- 根据用户图片及公开资料调整串列座舱、发动机、短翼、尾梁、起落架和 Longbow 雷达的比例与位置。
- 增加十九管 M261 火箭巢、四联 Hellfire 导弹挂架和外置副油箱，支持 Mixed / Combat / Ferry 三套载荷布局；
  每个短翼挂点只承载一种载荷。挂点与局部结构属于公开资料下的外形近似，不是实机精确工程坐标。
- 细化机头 TADS / PNVS 分层传感器、光学窗口、支架、枢轴、座舱边框与雨刷，以及机炮吊架和供弹链。
- 增加发动机进气风扇、轮毂螺栓、环面轮胎、检修分缝、紧固件、航行灯及油箱附件。
- 提高曲面细分，加入保形插值、平滑法线和非均匀缩放下的逆转置法线变换；装甲与舱窗保留硬边。
  增加的是 VBO 内的顶点数据，位置 / 颜色 / 法线仍使用三个 VBO。
- 默认 Mixed 完整几何为 315,330 个顶点、105,110 个三角形；部件拆分和尾桨复用后，GPU 唯一顶点数为 315,222。

### 部件动画与飞行物理

- 主旋翼、尾桨和水平尾翼拆为独立可动部件；四份 Mesh 输出七个 DrawItem，主画面和阴影使用同一套部件变换。
- 主旋翼与剪刀式尾桨持续旋转；Q/E 控制尾桨变距，Z/C 控制水平尾翼偏转，松键后平滑回中。
- 新增 `glframework/physics/`，集中管理刚体状态、驾驶输入、飞行参数与动力学；以 120 Hz 固定步长计算
  重力、推力、阻力、姿态力矩和三点起落架接地，并对渲染姿态插值。
- 发动机开关带旋翼转速与升力的平滑响应；进入驾驶后，R 控制发动机；首次驾驶前仍可用 R 暂停旋翼展示。
- 物理内部使用米、秒、千克，由载具对象转换到场景坐标；采用辅助悬停与速度控制，未校准到实机性能。
- 飞行时阴影视锥跟随载具，退出驾驶后保留物理状态，飞机继续减速悬停或在停机后下落。

### 驾驶交互与第三人称相机

- G 在座舱附近进入驾驶，驾驶中可随时退出；按键按下沿触发，避免长按反复切换。
- 进入驾驶检查座舱锚点的三维距离，默认约 5 个物理米；窗口标题显示距离和可驾驶提示。
- 修正驾驶输入：WASD 沿飞机实际水平朝向移动，Q/E 独立转向；鼠标只旋转观察视角，不改变飞机航向或移动轴。
- 支持空格 / 左 Ctrl 或 Page Up / Page Down 升降、Shift 提高目标速度、滚轮缩放跟随距离。
- 新增 `ThirdPersonCamera.*`，平滑跟随飞机；俯仰范围放宽至 -85°～+85°，允许从下方观察，
  仍保留跟随距离和镜头最低世界高度限制。

### 实时速度显示与调参说明

- 新增 `glframework/ui/Speedometer.*` 及 `asset/shader/ui/speedometer.vert` / `.frag`，
  在三维场景绘制后叠加右上角速度面板；使用系统字体，不增加外部字体依赖。
- 读取飞机真实三维线速度，换算为 km/h 并保留一位小数，包含升降速度；驾驶与自由观察均显示，未启用物理时显示 0.0。
- 明确 `HelicopterParameters::maxSpeed` 使用 m/s，是常规目标水平速度；Shift 将目标提高到 1.6 倍。
  实际速度还受最大倾角、推力和阻力限制，仅增大目标值不保证提速。240 km/h 对应 `240.0f / 3.6f` m/s。
  本轮未加入阻力补偿，也未将此参数改成直接设定实际速度的开关。
- 保留当前飞行调参：`maxSpeed=240` m/s、`linearDrag=0.0010`、`quadraticDrag=0.0008`、`spoolResponse=1.5`；
  这些是演示参数，不代表 AH-64D 实机性能。

### 文档、调用索引与验证状态

- 新增 `docs/AH64D_REFERENCE.md`、`docs/AH64D_ANIMATION.md`、`docs/AH64D_FLIGHT.md` 和
  `docs/VEHICLE_CODE_MAP.md`，补充参考来源、按键、单位、文件职责、调用链和调参入口。
- 为几何、动画、飞行物理、驾驶交互、相机及速度显示添加中文检索标签注释，并更新 README 操作说明与截图。
- 模型与动画阶段已进行几何、法线、部件转轴及物理集成检查；速度面板已完成 Debug 构建，并验证静止与运动时的实际渲染。
- 修正停机测试的固定等待时间：依据发动机响应参数等待至少五个时间常数，继续检查转速低于 1% 和失去升力，
  不修改当前飞行参数。修正后几何与物理两项测试均通过，程序已实际启动验证。
- 发布前补充构建目录、环境配置和凭据文件的忽略规则，避免将本地生成内容上传。

以下保留场景对象化重构（P0 + P1）的初始实现记录：把散落在 `Application` 里的骰子 / 地面 / 灯光收敛成对象。

### 新增场景层 `glframework/scene/`

- `DrawItem.h`：一次绘制的完整描述（网格、模型矩阵、材质、点数、索引段、阴影剔除方式）。
  这是「场景」与「渲染器」之间唯一的契约。
- `SceneObject.h`：对象接口 `appendDrawItems / appendShadowItems / castsShadow / opacity`。
- `MeshObject.*`：通用「网格 + 一套材质」对象，地面即它的一个实例。
- `DiceObject.*`：骰子——单位立方体（24 顶点/36 索引）、六面材质、点数、自转（角度+速度+轴）、
  不透明度全部内聚；`createCubeMesh()` 返回 `shared_ptr<Mesh>`，多颗骰子可共享几何。
- `Scene.*`：对象容器，负责遍历收集绘制项与阴影强度近似。
- `MeshBuilder.*`（`glframework/geometry/`）：程序化几何累加器——盒体 / 圆柱 / 椭圆截面放样；
  每个三角形独立顶点（平面法线），绕序按「面心相对外法线」自动纠正。
- `HelicopterObject.*`：程序化低多边形直升机（放样机身、后倾风挡、尾梁、垂尾/平尾、四叶主旋翼、
  尾桨、滑橇），1380 顶点 / 460 三角形；一份网格 + 顶点色区分部件。初始场景放在骰子右侧并投阴影，
  后续已由上述 AH-64D 模型替换。

### 新增 `glframework/lighting/LightRig.*`

- 方向光参数 + 光源正交矩阵 + 太阳方向（`sunDirection() = -光传播方向`）收拢到一处。
- `ShadowMap` 不再提供 `lightSpaceMatrix()`，只负责深度纹理/帧缓冲。

### `Application` 变化

- 成员从「6 面材质数组 + 点数数组 + 不透明度 + 自转三件套 + 两块 Mesh + 两个模型矩阵」
  收敛为 `Scene m_scene` + `LightRig m_lightRig` + 两个非拥有指针（`m_dice` / `m_ground`）。
- `renderScene()` 变成两个通用 pass：收集 `DrawItem` → 深度 pass 遍历投射者 →
  主 pass 先画不透明、再按距离倒序画半透明（`std::stable_sort`）。
- 公开接口签名不变（`setCubeFaceMaterial` / `setObjectOpacity` / 自转接口等全部转发）；
  但**这些接口现在必须在 `initialize()` 之后调用**（对象在那时创建），提前调用会打印提示。
- 删除 `createCubeGeometry` / `createGroundGeometry` / `updateCubeModel` / `wrapSpinAngle`。

### 验证

- 重构前后用 `--no-spin --screenshot` 对比 4 个场景（默认 / 半透明 / 关阴影 / 太阳视角），
  在「旋转轴与 HEAD 一致」的前提下**逐像素完全一致**；斜轴 `(1,1,1)` 是工作区里的未提交改动，
  重构保留了它，因此与 HEAD 渲染有差异属于预期。
  实窗验证：自转推进、空格暂停/恢复、stderr 为空。
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

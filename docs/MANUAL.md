# RenderingCube 使用说明书

> 一颗会转的骰子放在巨大的地面片上：OpenGL 3.3 Core + GLFW + GLAD + GLM，
> Cook-Torrance PBR、方向光阴影、程序化天空、距离雾、逐面材质与程序化点数。
> 本文对应 **v0.12**（2026-10-31）。载具建模、动画与驾驶的补充说明见
> [飞行说明](AH64D_FLIGHT.md) 和 [载具代码调用索引](VEHICLE_CODE_MAP.md)。
> 版本变更记录见 `CHANGELOG.md`。

---

## 1. 快速开始

**前置**：Visual Studio 2022（C++ 桌面工作负载）、CMake ≥ 3.20。依赖已随仓库提供，无需另外安装。

```powershell
# 构建（Debug）
.\skills\rendering-cube-pbr\scripts\verify_project.ps1 -ProjectRoot .

# 运行
.\build\Debug\RenderingCube.exe
```

等价的手工命令：

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' -S . -B build -G 'Visual Studio 17 2022' -A x64
& 'C:/Program Files/CMake/bin/cmake.exe' --build build --config Debug
```

跑一次就退出、不弹窗口（用于出图/回归）：

```powershell
.\build\Debug\RenderingCube.exe --no-spin --screenshot shot.bmp
```

---

## 2. 目录结构：每个分区负责什么

```text
D:\DSH\cube
├─ application/            程序骨架（窗口、输入、场景、渲染顺序、对外接口）
│  ├─ Application.h         对外契约：全部 setter/getter + 私有状态声明
│  └─ Application.cpp       实现：GLFW 生命周期、双 pass 渲染、离屏截图、BMP 写盘
├─ glframework/            可复用渲染零件（不认识"骰子/地面"这些业务概念）
│  ├─ Shader.*              GLSL 编译链接、uniform setter、驱动日志
│  ├─ Camera.*              第一人称相机 + 滚轮指数平滑升降
│  ├─ ShadowMap.*           深度纹理 + 深度-only FBO（不管光照几何）
│  ├─ SkyDome.*             程序化天空球（内部复用 Mesh）
│  ├─ SunSprite.*           可选太阳图层：billboard + 程序化/贴图材质
│  ├─ geometry/Mesh.*       VAO + position/color/normal VBO + EBO，支持按索引区间绘制
│  ├─ geometry/MeshBuilder.* 程序化几何累加：盒体/圆柱/放样，绕序按外法线自动纠正
│  ├─ scene/                 场景层：只产出 DrawItem，不认识渲染器
│  │  ├─ DrawItem.h          一次绘制的完整描述（几何/变换/材质/点数/索引段/剔除方式）
│  │  ├─ SceneObject.h       对象接口：appendDrawItems / appendShadowItems / opacity
│  │  ├─ MeshObject.*        通用"网格 + 一套材质"对象（地面就是它）
│  │  ├─ DiceObject.*        骰子：单位立方体 + 六面材质 + 点数 + 自转（可共享网格）
│  │  ├─ HelicopterObject.*  程序化直升机：放样机身 + 尾梁 + 主/尾旋翼 + 滑橇（1380 顶点/460 三角）
│  │  └─ Scene.*             对象容器：遍历收集、阴影强度近似
│  └─ lighting/
│     ├─ DirectionalLight.h 方向 + 辐照度（没有物理位置）
│     ├─ PBRMaterial.h      材质参数 + clamped() 夹紧
│     ├─ LightRig.*         灯光设备：方向光 + 光源正交矩阵 + 太阳方向
│     └─ PBRRenderer.*      PBR Shader 与全部 uniform 命名、逐面/逐点数 draw
├─ asset/shader/           运行时按绝对路径加载的 GLSL（不编译进 exe）
│  ├─ pbr/pbr.vert         世界/对象空间输出 + 法线逆转置
│  ├─ pbr/pbr.frag         BRDF、阴影、天空反射、雾、骰子点数
│  ├─ shadowDepth.vert/.frag  深度 pass（无颜色附件）
│  ├─ skyDome.vert/.frag   去平移的天空球，xyww 压远平面
│  ├─ sun.vert/.frag       太阳 billboard：日盘 + 光晕 + 放射光芒
├─ asset/texture/           运行期贴图（sun_soft.bmp 为太阳示例材质）
├─ src/main.cpp            命令行解析 → 调 Application 接口；本身不持有状态
├─ third_party/            用户提供的依赖，版本不替换
│  ├─ glfw/                GLFW 3.5.1 预编译静态库（x64, Debug CRT）
│  ├─ glad/                GL 3.3 core 加载器源码（编译成静态库）
│  └─ glm/                 header-only 数学库
├─ docs/                   截图、接触处特写、本说明书
├─ skills/rendering-cube-pbr/  本项目遵循的技能包（含修好的构建校验脚本）
├─ CMakeLists.txt          构建定义（依赖校验、/utf-8、/MDd、绝对资源路径）
├─ README.md               项目说明（偏"给人看"）
├─ CHANGELOG.md            版本变更记录（当前 v0.11）
├─ .gitignore / .gitattributes
└─ build/                  CMake 生成物，可整目录删除后重建
```

各分区的边界与"不该放什么"：

| 分区 | 职责 | 不该出现 |
|---|---|---|
| `application/` | 窗口生命周期、事件回调、每帧调度、渲染顺序、对外配置接口 | 裸 `glGenBuffers`、BRDF 数学、着色器源码字符串 |
| `glframework/` | 可复用零件：着色器、相机、网格、阴影、天空、材质 | 场景布局、骰子点数这种业务规则 |
| `asset/shader/` | GLSL 源码，运行期加载 | C++ 代码 |
| `src/main.cpp` | 解析命令行、调用接口、决定 run 还是截图 | 任何成员状态、矩阵计算 |
| `third_party/` | 用户提供的第三方库 | 业务代码 |
| `docs/` | 截图与文档 | 可执行文件 |
| `build/` | 构建产物 | 手改的源码 |

---

## 3. 渲染流程（每帧两遍）

```text
run()
 ├─ processInput()           键盘/鼠标；空格边沿检测切换自转
 ├─ camera.updateElevation() 滚轮目标高度的指数平滑
 ├─ 自转积分                 m_spinAngle += speed * dt（仅启用时）
 ├─ updateCubeModel()        T(0, height, 0) · R_y(angle) · S(0.8)
 └─ renderScene()
     ├─ Pass 1 深度 pass（光源视角）
     │    bind 阴影 FBO(2048²) → 清深度 → 深度着色器
     │    只画立方体，且只画背面（glCullFace(GL_FRONT)）
     │    地面不写深度：它是纯接收者，写进去只会自阴影
     └─ Pass 2 主 pass
          恢复窗口 FBO/视口 → 清屏 → 天空球（深度写关闭）
          → 地面（不透明 PBR）→ 立方体 6 个面各 draw 一次
          （opacity < 1 时最后一个阶段开启混合、关闭深度写、只画正面）
```

关键不变量：

- **同一份 `m_cubeModel` 同时喂给阴影 pass 和主 pass**，所以本体、影子、点数永远同步。
- 天空最先画且不写深度；地面先于骰子；半透明骰子最后画。
- 阴影 pass 只写投射者，因此不需要 polygon offset，接触处不会出现"亮缝"。

---

## 4. 现有功能清单

| 功能 | 触发方式 | 实现位置 |
|---|---|---|
| 窗口 + HiDPI 正确缩放 | 启动即用，拖动窗口自动重设 viewport | `Application::createWindow / onFramebufferSize` |
| 4× MSAA | 启动即用 | `GLFW_SAMPLES=4` + `glEnable(GL_MULTISAMPLE)` |
| 第一人称相机 | 鼠标转向、WASD、PgUp/PgDn、Shift 加速 | `Camera` + `Application::processInput` |
| 滚轮升降（帧率无关的平滑） | 滚轮，每格 0.25，指数平滑 k=14 | `Camera::addElevation / updateElevation` |
| 骰子：每面独立材质 | 默认六色；`setCubeFaceMaterial` 单独改 | `Application` 六面数组 + `PBRRenderer::draw` 索引区间 |
| 骰子点数 1..6 | 默认开；`--no-pips` 关 | `pbr.frag` 的 `dicePipMask()`（对象空间 3×3 网格 + 位掩码） |
| 自转 | 默认 0.6 rad/s；空格暂停/继续；`--no-spin`、`--spin-angle <度>` | `DiceObject`（角度+速度+轴） |
| 场景对象化 | 常开 | `Scene` 收集 `DrawItem`，`DiceObject` / `MeshObject` / `HelicopterObject` 只管自己；灯光归 `LightRig` |
| 程序化直升机 | 默认出现在骰子右侧 | `HelicopterObject` + `MeshBuilder`（启动时生成，无外部模型文件） |
| Cook-Torrance PBR | 常开 | `pbr.frag`（GGX / Smith-Schlick / Schlick Fresnel / 能量守恒） |
| 方向光 + 2048² 阴影 | 默认开；`--no-shadow` 关 | `ShadowMap` + `shadowDepth.*` + `pbr.frag` 的 3×3 PCF |
| 程序化天空 | 常开 | `SkyDome` + `skyDome.*` |
| 太阳图层（可选） | 默认开；`--sun` / `--sun-texture` 调整 | `SunSprite` + `sun.*`（独立 billboard，位置跟随光方向） |
| 环境反射近似 | 强度 0.55；`--no-reflection` 关 | `pbr.frag` 复用 `proceduralSky()` + 粗糙度感知 Fresnel |
| 距离雾 | 常开（密度 0.0022） | `pbr.frag` 末尾，向地平线色混合 |
| 半透明 | `--transparent` / `--opacity <0..1>` | `Application::renderScene` 透明分支 |
| 离屏截图（逐像素可复现） | `--screenshot <file.bmp>` | `Application::captureFrame` + `writeBmp24` |
| 调试覆盖 | `--roughness`、`--metallic`（保留每面颜色） | `src/main.cpp` |
| 启动日志 | 打印 GL 版本/渲染器、窗口与 framebuffer 尺寸 | `Application::createWindow` |

---

## 5. 操作方式

| 输入 | 行为 |
|---|---|
| 鼠标 | 视角（yaw / pitch，pitch 限制 ±89°） |
| W / S | 前后（沿水平前向，抬头不会起飞） |
| A / D | 左右 |
| Page Up / Page Down | 世界 Y 方向升降 |
| 鼠标滚轮 | 世界 Y 方向升降（每格 0.25，平滑过渡） |
| Left Shift | 移动加速 ×2.5 |
| 空格 | 暂停 / 继续骰子自转（边沿触发，按住不连续翻转） |
| Esc | 退出 |

---

## 6. 命令行参数

| 参数 | 作用 |
|---|---|
| `--transparent` | 不透明度 0.62 |
| `--opacity <0..1>` | 指定不透明度（与每面自身 opacity 相乘） |
| `--roughness <value>` | 覆盖六面粗糙度，**保留每面颜色** |
| `--metallic <0..1>` | 覆盖六面金属度，**保留每面颜色** |
| `--no-pips` | 不画骰子点数 |
| `--no-spin` | 启动时不转（运行中仍可按空格开启） |
| `--spin-angle <度>` | 固定自转角度（截图逐像素可复现的关键） |
| `--no-shadow` | 关闭阴影 pass |
| `--no-reflection` | 关闭环境反射 |
| `--no-mouse` | 释放鼠标并忽略视角增量（截图/演示） |
| `--hidden` | 隐藏窗口（仍正常渲染） |
| `--screenshot <file.bmp>` | 离屏渲染一帧、写 24 位 BMP 后退出 |
| `--sun <radiance>` | 太阳亮度（默认 26，越大核心越白） |
| `--sun-texture <file.bmp>` | 太阳改用 BMP 贴图（约定：亮度当 alpha） |
| `--camera x y z yaw pitch` | 直接摆放相机（角度制），出图/调试用 |

---

## 7. C++ 配置接口（`Application`）

```cpp
Application app(1400, 900, "RenderingCube | OpenGL 3.3 PBR");

// 灯光：方向（光传播方向）+ 颜色 + 强度
app.setLightDirection(glm::vec3(-0.55f, -1.0f, -0.42f));
app.setDirectionalLight(light);

// 材质：六面统一 / 单面 / 读取
app.setCubeMaterial(material);                    // 六面同一材质
app.setCubeFaceMaterial(0, material);             // 0=+Z,1=-Z,2=+X,3=-X,4=+Y,5=-Y
PBRMaterial face2 = app.cubeFaceMaterial(2);      // 读-改-写
app.setGroundMaterial(groundMaterial);
app.setPipsEnabled(false);                        // 关点数

// 视觉
app.setReflectionStrength(0.55f);                 // [0,1]
app.setObjectOpacity(0.62f);                      // [0,1]，<1 走半透明路径
app.enableShadows(true);

// 自转
app.setCubeSpinEnabled(false);
app.toggleCubeSpin();
app.setCubeSpinSpeed(0.6f);                       // 弧度/秒，可负（反向），夹紧 ±20
app.setCubeSpinAngle(glm::radians(28.0f));        // 截图前定死角度

// 运行环境
app.setMouseLookEnabled(false);                   // 释放鼠标
app.setWindowVisible(false);                      // 必须在 initialize() 之前
app.initialize();
app.run();

// 离屏截图（不进入主循环）
app.captureFrame("shot.bmp", 1400, 900);
```

太阳是独立图层，接口只有四个（详见 §14）：

```cpp
SunStyle style = app.sunStyle();                  // 取当前参数
style.radiance = 40.0f;                           // 改亮度/角半径/光芒数…
app.setSunStyle(style);
app.setSunTexture("asset/texture/sun_soft.bmp");  // 换成贴图材质
app.clearSunTexture();                            // 回到程序化材质
```

材质定义为 `albedo, metallic, roughness, ao, opacity`；`roughness` 由 `PBRMaterial::clamped()` 夹到 `[0.04, 1]`。

---

## 8. 调参速查表

| 想改什么 | 位置 | 说明 |
|---|---|---|
| 骰子高度 | `application/Application.cpp:24` `kCubeCenterHeight` | 底面高度 = 该值 − `kCubeScale`(0.8)。**当前源码为 1.95（底面离地 1.15）**，抬高越多脚下阳光缝隙越宽 |
| 骰子大小 | `kCubeScale`（`Application.cpp:23`） | 单位立方体 ±1 乘以它；边长 = 2 × 该值 |
| 自转速度 | `DiceObject` 成员 `m_speed`(0.6) / `setSpinSpeed()` | 弧度/秒，可负（反向），夹紧 ±20 |
| 初始自转角 | `Application.cpp` 的 `kCubeInitialSpinDegrees`(28) | 传给 `DiceObject` 构造 |
| 自转轴 | `Application.cpp:26` 的 `kCubeSpinAxis`，或运行期 `DiceObject::setSpinAxis()` | 当前工作区是 (1,1,1)（斜轴翻滚）；改 (0,1,0) 即绕竖轴 |
| 相机初值 | `Application::Application` 里的 `setPosition / setYawPitch` | 默认 (0, 3, 6.6)、yaw −90°、pitch −20° |
| 移动速度 / 鼠标灵敏度 | `glframework/Camera.h` 的 `m_moveSpeed`(4.0) / `m_mouseSensitivity`(0.1) | — |
| 滚轮平滑快慢 | `glframework/Camera.cpp:14` `kElevationSmoothing`(14) | 越大越跟手；8 更绵，25 几乎瞬时 |
| 光照方向/颜色/强度 | `glframework/lighting/DirectionalLight.h` 或 `setLightDirection()` | 方向是"光传播方向" |
| 阴影分辨率 | `application/Application.cpp:18` `kShadowMapSize`(2048) | 提到 4096 边缘更锐，代价 64MB 显存 |
| 阴影覆盖范围 | `glframework/lighting/LightRig.cpp` `kOrthoHalfExtent`(9) | 收得越紧纹素越细；太小会让远处阴影被裁掉 |
| 阴影深度范围 | 同上 `kDepthNear/Far`(15/21) | 必须罩住光源相机到场景的距离（`kLightCameraDistance`=18） |
| 阴影偏移 | `pbr.frag` 的 `bias = max(0.0022*(1-NdotL), 0.0006)` | 调大消 acne 但会让影子"缩离"物体 |
| 天空颜色 | `pbr.frag` **和** `skyDome.frag` 的 `proceduralSky()` | **两处必须逐字一致**，否则背景与物体反射对不上 |
| 雾浓度 | `pbr.frag` 的 `exp(-distance * 0.0022)` | 只影响地面远处融合 |
| 点数大小/间距 | `pbr.frag` 的 `pipRadius`(0.15) / `gridOffset`(0.57) | 单位是对象空间 ±1 的面坐标 |
| 地面反照率/尺寸 | `Application::createScene()` 里的 `groundMaterial` / `kGroundHalfExtent`(1200) | 地面 roughness 保持 0.90，避免大块白色高光 |
| 太阳亮度/尺寸/光芒 | `glframework/SunSprite.h` 的 `SunStyle` 默认值 | radiance 26、coreDegrees 1.4°、glowDegrees 20°、rayCount 12、rayStrength 0.55 |
| 太阳材质 | `application.setSunTexture("asset/texture/*.bmp")` | 亮度当 alpha；仅支持未压缩 24/32 位 BMP |
| 是否绘制太阳 | `Application::renderScene` 里的 `m_sun.render(...)` 一行 | 删掉这一行即整体移除太阳 |
| 窗口尺寸/标题 | `src/main.cpp` 的 `Application(1400, 900, ...)` | — |

---

## 9. 已知近似与限制

- **阴影**：单方向光、无级联；正交框只覆盖 ±9，范围外按全受光。地面不写深度（纯接收者）。
- **点数**：是画在 albedo 上的图案，没有凹陷、不扰动法线、不投自己的影子。
- **环境反射**：复用同一份程序化天空 + 粗糙度感知 Fresnel，是近似，不是预滤波 IBL。
- **太阳**：独立 billboard，没有写进 `proceduralSky()`，所以骰子的环境反射里看不到它；
  也没有 bloom / 屏幕空间眩光后处理。
- **半透明**：只有透射，没有折射；不排序，且只画正面一层。
- **截图**：离屏 FBO 不带 MSAA，所以 `--screenshot` 的锯齿比窗口更明显（内容一致）。
- **构建**：随项目的 `glfw3.lib` 是 Debug CRT，所有配置统一 `/MDd`，Release 也一样。

---

## 10. 常见问题

| 现象 | 原因 / 处理 |
|---|---|
| 骰子整块灰、看不到每面颜色 | 已被逐面材质取代；若又出现，检查 `pbr.frag` 是否用了 `vColor` 且各面材质是否被 `setCubeMaterial()` 覆盖成同一套 |
| 骰子脚下一圈亮缝 | 立方体离地太高（见 §8 第一行），或阴影被 polygon offset 推开；当前深度 pass 只画投射者、无 offset |
| 立方体表面有密集条纹/斑点 | 阴影 acne：检查阴影 pass 是否仍只画背面，`bias` 是否过小 |
| 关掉阴影后仍有条纹 | 那是几何自交问题（面的顶点没有沿法线偏移），不是阴影 |
| `--screenshot` 每次结果不同 | 自转在跑：加 `--no-spin`；需要特定姿态再加 `--spin-angle <度>` |
| 改了天空颜色但背景没变 | 只改了一处：`pbr.frag` 与 `skyDome.frag` 的 `proceduralSky()` 必须同步 |
| 链接报 LNK2038 RuntimeLibrary | GLFW 是 Debug CRT，必须全配置 `/MDd` |
| 看不到太阳 | 太阳在光方向的反侧，默认相机背对它；用 `--camera 0 3 6.6 37.1 55.1` 转过去 |
| `--sun-texture` 没效果 | 只支持未压缩 24/32 位 BMP；看 stderr 的 `[SunSprite]` 日志 |
| 运行时报 GLFW 错误/黑窗口 | 需要支持 OpenGL 3.3 Core 的驱动；看 stderr 的 `[Shader]` / `[ShadowMap]` 日志 |

---

## 11. 验证方式

```powershell
# 构建校验（会清一遍再编译，并确认 exe 是新产出的）
.\skills\rendering-cube-pbr\scripts\verify_project.ps1 -ProjectRoot .

# 视觉回归：同一参数两次输出应字节完全一致
.\build\Debug\RenderingCube.exe --no-spin --screenshot a.bmp
.\build\Debug\RenderingCube.exe --no-spin --screenshot b.bmp
Get-FileHash a.bmp, b.bmp      # 两个 SHA256 应相同
```

需要人工确认的项：

- 三个可见面颜色不同、点数正确（1 / 4 / 5），自转时点数跟着面走不歪。
- 立方体表面无条纹状 acne，地面无过曝白斑。
- 阴影边缘无明显锯齿，且与立方体底部贴合。
- 转动相机时天空只旋转、不平移。
- 太阳与阴影方向一致：改 `setLightDirection()` 后，太阳位置与影子同时变化。
- `--transparent` 时能透出地面与阴影，阴影随不透明度变浅。

---

## 12. 关于 `docs/` 里的截图

`docs/*.png` 由 `--no-spin --screenshot` 生成，拍摄时 `kCubeCenterHeight = 0.805`（底面离地 5mm、阴影贴合）。
当前源码里该常量被手动改成 `1.95`，所以**现有截图与当前构建不一致**（现在骰子是悬空的）。
要恢复一致，二选一：

1. 把 `kCubeCenterHeight` 改回 `0.805f`；
2. 或重新出图：

   ```powershell
   .\build\Debug\RenderingCube.exe --no-spin --screenshot docs\latest.bmp
   ```

   注意：`--screenshot` 输出的是 **24 位 BMP**（扩展名写 .png 也只是把 BMP 字节改名）。
   要替换 `docs/*.png`，把 BMP 用看图工具另存为 PNG 即可。

---

## 13. 关于 `skills/` 目录

`skills/rendering-cube-pbr/` 是本项目的技能包（architecture / workflow / verify 脚本）。
其中 `scripts/verify_project.ps1` 已改成 ASCII 版并增加"产物必须重新生成"的校验：
原版在 Windows PowerShell 5.1 下会因无 BOM 的中文注释吞掉换行，导致**构建命令整条被注释、脚本只 configure 却永远报 passed**。
`D:\DSH\skills` 下的原版仍是旧脚本，若要复用请同样处理。
---

## 14. 太阳图层（SunSprite，可选模块）

**文件**：`glframework/SunSprite.h` / `SunSprite.cpp`，`asset/shader/sun.vert` / `sun.frag`，
示例贴图 `asset/texture/sun_soft.bmp`。

**它是什么**：一个朝向相机、放在"无穷远方向"上的四边形，画出日盘 + 光晕 + 放射光芒。
位置完全由光方向决定：`sunDir = -light.direction`，所以太阳、阴影、骰子高光永远自洽，
不需要额外的"太阳位置"状态。

**为什么这样设计（对应三条要求）**：

| 要求 | 落地方式 |
|---|---|
| 兼容性强 | 只用 GL 3.3 core 和工程已有的 `Shader` 类，不加任何第三方库；离屏截图依然逐像素可复现 |
| 依赖小 | 不改 `SkyDome` / `PBRRenderer` / 任何现有着色器。`Application` 只多一个成员 + 一行 `m_sun.render(...)`；删掉这行就等于删掉太阳。太阳着色器缺失只禁用太阳，不会让启动失败 |
| 可拓展 / 换材质 | ① 参数：`SunStyle{ tint, radiance, coreDegrees, glowDegrees, rayCount, raySharpness, rayStrength }`；② 贴图：`setSunTexture("xxx.bmp")`，约定"亮度当 alpha"（白不透明、黑透明），支持未压缩 24/32 位 BMP，无需图片解码库 |

**C++ 用法**：

```cpp
application.setSunStyle(style);                          // 调参数
application.setSunTexture("asset/texture/sun_soft.bmp"); // 换贴图材质
application.clearSunTexture();                           // 回到程序化材质
const SunStyle& current = application.sunStyle();
```

**命令行**：

| 参数 | 作用 |
|---|---|
| `--sun <radiance>` | 改亮度（默认 26） |
| `--sun-texture <path.bmp>` | 换成贴图材质 |
| `--camera x y z yaw pitch` | 直接摆相机（yaw/pitch 单位为度），方便把太阳放进画面 |

**已知取舍**：太阳没有写进 `proceduralSky()`，所以骰子的环境反射里看不到它——这是"低耦合"的代价。
要让它进入反射，就必须改 `pbr.frag` 的 `proceduralSky()`，那会破坏"零依赖"。

**截图**：`docs/renderingcube-sun.png`（程序化）、`docs/renderingcube-sun-texture.png`（贴图）。
复现命令：

```powershell
.\build\Debug\RenderingCube.exe --no-spin --camera 0 3 6.6 37.1 55.1 --screenshot sun.bmp
```

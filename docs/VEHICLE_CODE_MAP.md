# 载具新增代码调用索引

本索引用于查找直升机建模后新增的几何细节、曲面平滑、部件动画、飞行物理和驾驶交互代码。
在编辑器中全局搜索方括号注释标签或下面的完整函数名，即可定位入口。

## 先找功能入口

| 要找的内容 | 文件 / 函数 | 搜索标签 |
| --- | --- | --- |
| 创建直升机并交给场景管理 | `application/Application.cpp`：`createScene` | `[载具创建]` |
| 每帧调用顺序 | `application/Application.cpp`：`run` | `[载具更新]` |
| G 进入 / 退出、距离校验 | `application/Application.cpp`：`setDriving`、`processInput` | `[驾驶交互]` |
| 交互距离和座舱位置 | `glframework/scene/AttackHelicopterObject.cpp`：`interactionRadius`、`interactionPoint` | `[交互调参]` |
| WASD、Q/E、升降、发动机 | `application/Application.cpp`：`processInput` | `[驾驶输入]` |
| 机头相对移动方向 | `glframework/scene/AttackHelicopterObject.cpp`：`movementDirection` | `[驾驶输入]` |
| 鼠标 / 滚轮分流 | `application/Application.cpp`：`onCursorPosition`、`onScroll` | `[第三人称相机]` |
| 镜头俯仰、缩放、防穿地、跟随速度 | `glframework/ThirdPersonCamera.cpp` | `[相机调参]` |
| 力、力矩、固定步长、地面接触 | `glframework/physics/HelicopterDynamics.cpp` | `[飞行物理]` |
| 质量、惯量、最大速度和响应增益 | `glframework/physics/HelicopterParameters.h` | `[飞行物理]` |
| 场景单位与米制坐标转换 | `glframework/scene/AttackHelicopterObject.cpp`：`resetPhysicsFromModel` | `[坐标换算]` |
| 旋翼和尾翼动画 | `glframework/scene/AttackHelicopterObject.cpp`：`update`、`partTransforms` | `[部件动画]` |
| 机头、座舱、挂载和机炮形状 | `glframework/geometry/AttackHelicopterGeometry.cpp` | `[几何细节]` |
| 四份部件网格的生成和上传 | `glframework/geometry/AttackHelicopterGeometry.cpp`：`buildParts`、`createMeshes` | `[几何生成]` / `[几何上传]` |
| 平滑圆柱、放样、轮胎 | `glframework/geometry/MeshBuilder.h/.cpp` | `[曲面平滑]` |
| 飞行中的阴影视锥跟随 | `glframework/lighting/LightRig.h/.cpp`：`setShadowFocus` | `[阴影跟随]` |
| 右上角速度显示 | `glframework/ui/Speedometer.h/.cpp`；`Application::renderScene` 最后调用 | `[速度显示]` |

## 初始化调用链

```text
src/main.cpp
  Application::initialize
    createWindow                        建立 GL 上下文
    loadShaders
    createScene
      AttackHelicopterGeometry::createMeshes
        buildParts                      CPU 生成机身、主旋翼、尾桨单叶和水平尾翼
        uploadPart × 4                  仅初始化时上传网格
      Scene::add<AttackHelicopterObject> 场景拥有载具及网格
  setCameraPose                         若指定了 --camera，先应用位置
  setDriving(true)                      若指定了 --drive，仍检查座舱距离
```

网格创建需要 GL 上下文。`releaseResources` 在销毁窗口之前清空 `Scene`，确保 GPU 资源及时释放。

## 一帧的调用链

```text
Application::run
  processInput
    G 按下边沿 -> setDriving -> canEnterFrom
    驾驶：组装 FlightControls -> AttackHelicopterObject::setFlightControls
    自由：Camera::moveLocal，或清空上一次驾驶输入
  AttackHelicopterObject::update
    HelicopterDynamics::setControls
    HelicopterDynamics::advance
      step，固定 1/120 秒
        转速响应 -> 辅助飞控 -> 力矩/姿态 -> 合力/位移 -> resolveGround
    interpolatedState -> 米制转换 -> 飞机整体 Model
    旋翼相位、尾桨变距、水平尾翼动画
  updateVehicleCamera
    LightRig::setShadowFocus
    ThirdPersonCamera::update 或自由模式交互提示
  renderScene
    Scene 输出 DrawItem / ShadowItem
    partTransforms 计算机身和可动部件变换
    阴影 pass -> 主 pass
    Speedometer::render                 叠加右上角速度，三维物理速度 × 3.6 转 km/h
```

物理和动画只在载具 `update` 中推进一次。两个渲染 pass 读取相同姿态，不能分别推进时间。

## 驾驶输入的边界

- `m_pilotHeading` 是飞机目标航向，只有 Q/E 更新。鼠标不写这个值。
- WASD 调用 `AttackHelicopterObject::movementDirection`，使用飞机实际机头的水平投影。
- `ThirdPersonCamera::movementDirection` 是观察方向辅助函数，当前驾驶控制不调用它。
- 升降单独写入 `FlightControls::climb`，与观察俯仰、机头倾角分开。
- `setDriving(true)` 先检查完整三维座舱距离，再修改状态；`false` 退出不检查距离。
- 退出驾驶调用 `clearFlightControls`，保留物理状态并刹停悬停；发动机关闭时仍会下落。

## 常改参数

| 参数 | 位置 | 当前含义 |
| --- | --- | --- |
| 交互点 | `AttackHelicopterObject::interactionPoint` | 机体局部 `(0.65, 0.50, 0)` |
| 交互范围 | `AttackHelicopterObject::interactionRadius` | 2.5 个几何单位，约 5 物理米 |
| 初始镜头 | `ThirdPersonCamera::enter` / 私有字段 | 默认俯角 -18°、距离 5.8 场景单位 |
| 镜头俯仰 | `ThirdPersonCamera::mouseMovement` | -85°～85°，可从飞机下方观察 |
| 缩放 / 跟随 | `ThirdPersonCamera::zoom` / `update` | 距离 3.5～12、防穿地 Y=0.35、跟随系数 7 |
| 飞机转向速度 | `Application::processInput` | Q/E 每秒改变目标航向 70° |
| 视觉旋翼速度 | `AttackHelicopterObject` 私有字段 / `setRotorSpeeds` | 度/秒接口，内部为弧度/秒 |
| 物理参数 | `HelicopterParameters.h` | 演示参数，不是 AH-64 实机标定数据 |

更详细的使用说明见 [飞行与驾驶](AH64D_FLIGHT.md)、[部件动画](AH64D_ANIMATION.md)、[模型参考](AH64D_REFERENCE.md)。

速度面板的字体、边距和大小位于 `ui/Speedometer.cpp`，配色及圆角位于
`asset/shader/ui/speedometer.frag`。文字使用 Windows 系统字体，数值变化时更新纹理，
HUD 在三维场景后绘制并恢复 GL 状态。`loadShaders` 初始化，`releaseResources` 在窗口销毁前释放。

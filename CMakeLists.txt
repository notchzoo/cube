cmake_minimum_required(VERSION 3.20)
project(RenderingCube VERSION 0.12 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# 用户提供的依赖统一放在 third_party，不替换版本：
#   third_party/glfw   GLFW 3.5.1 预编译静态库（x64，Debug CRT）
#   third_party/glad   生成的 GL 3.3 core 加载器源码
#   third_party/glm    header-only 数学库
# ---------------------------------------------------------------------------
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(GLFW_ROOT "${THIRD_PARTY_DIR}/glfw")
set(GLAD_ROOT "${THIRD_PARTY_DIR}/glad")
set(GLM_ROOT "${THIRD_PARTY_DIR}/glm")

if(NOT EXISTS "${GLFW_ROOT}/lib/glfw3.lib")
    message(FATAL_ERROR "找不到 GLFW 静态库: ${GLFW_ROOT}/lib/glfw3.lib")
endif()
if(NOT EXISTS "${GLAD_ROOT}/src/glad.c")
    message(FATAL_ERROR "找不到 GLAD 源码: ${GLAD_ROOT}/src/glad.c")
endif()
if(NOT EXISTS "${GLM_ROOT}/glm/glm.hpp")
    message(FATAL_ERROR "找不到 GLM 头文件: ${GLM_ROOT}/glm/glm.hpp")
endif()

# GLFW：导入预编译静态库，头文件与库必须同源。
add_library(glfw_supplied STATIC IMPORTED)
set_target_properties(glfw_supplied PROPERTIES
    IMPORTED_LOCATION "${GLFW_ROOT}/lib/glfw3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${GLFW_ROOT}/include"
)

# GLAD：作为静态库编译，避免在多个翻译单元里重复实现。
add_library(glad STATIC "${GLAD_ROOT}/src/glad.c")
target_include_directories(glad PUBLIC "${GLAD_ROOT}/include")

set(RENDERINGCUBE_SOURCES
    src/main.cpp

    application/Application.h
    application/Application.cpp

    glframework/Shader.h
    glframework/Shader.cpp
    glframework/Camera.h
    glframework/Camera.cpp
    glframework/ThirdPersonCamera.h
    glframework/ThirdPersonCamera.cpp
    glframework/physics/RigidBodyState.h
    glframework/physics/FlightControls.h
    glframework/physics/HelicopterParameters.h
    glframework/physics/HelicopterDynamics.h
    glframework/physics/HelicopterDynamics.cpp
    glframework/ShadowMap.h
    glframework/ShadowMap.cpp
    glframework/SkyDome.h
    glframework/SkyDome.cpp
    glframework/SunSprite.h
    glframework/SunSprite.cpp
    glframework/ui/Speedometer.h
    glframework/ui/Speedometer.cpp

    glframework/geometry/Mesh.h
    glframework/geometry/Mesh.cpp
    glframework/geometry/MeshBuilder.h
    glframework/geometry/MeshBuilder.cpp
    glframework/geometry/AttackHelicopterGeometry.h
    glframework/geometry/AttackHelicopterGeometry.cpp

    glframework/lighting/DirectionalLight.h
    glframework/lighting/PBRMaterial.h
    glframework/lighting/PBRRenderer.h
    glframework/lighting/PBRRenderer.cpp
    glframework/lighting/LightRig.h
    glframework/lighting/LightRig.cpp

    glframework/scene/DrawItem.h
    glframework/scene/SceneObject.h
    glframework/scene/MeshObject.h
    glframework/scene/MeshObject.cpp
    glframework/scene/DiceObject.h
    glframework/scene/DiceObject.cpp
    glframework/scene/AttackHelicopterObject.h
    glframework/scene/AttackHelicopterObject.cpp
    glframework/scene/Scene.h
    glframework/scene/Scene.cpp

    asset/shader/pbr/pbr.vert
    asset/shader/pbr/pbr.frag
    asset/shader/shadowDepth.vert
    asset/shader/shadowDepth.frag
    asset/shader/skyDome.vert
    asset/shader/skyDome.frag
    asset/shader/sun.vert
    asset/shader/sun.frag
    asset/shader/ui/speedometer.vert
    asset/shader/ui/speedometer.frag
)

add_executable(RenderingCube ${RENDERINGCUBE_SOURCES})
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT RenderingCube)

# 让 Visual Studio 的筛选器树与磁盘目录保持一致。
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${RENDERINGCUBE_SOURCES})

target_include_directories(RenderingCube PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/application"
    "${CMAKE_CURRENT_SOURCE_DIR}/glframework"
    "${CMAKE_CURRENT_SOURCE_DIR}/glframework/geometry"
    "${CMAKE_CURRENT_SOURCE_DIR}/glframework/lighting"
    "${CMAKE_CURRENT_SOURCE_DIR}/glframework/scene"
    "${CMAKE_CURRENT_SOURCE_DIR}/glframework/physics"
    "${GLM_ROOT}"
)

# Shader 按绝对路径加载，运行目录不影响结果。
target_compile_definitions(RenderingCube PRIVATE
    RENDERINGCUBE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/asset"
)

target_link_libraries(RenderingCube PRIVATE
    glad
    glfw_supplied
    opengl32
    user32
    gdi32
    shell32
)

if(MSVC)
    # /utf-8：源码含中文注释，否则 MSVC 会按本地代码页解析。
    target_compile_options(RenderingCube PRIVATE /utf-8)

    # 随项目提供的 glfw3.lib 用 Debug CRT 构建；主程序若用 /MD 会报
    # LNK2038 RuntimeLibrary 不匹配，因此所有配置统一 /MDd。
    set_property(TARGET glad RenderingCube PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(AttackHelicopterGeometryTest
        tests/AttackHelicopterGeometryTest.cpp
        glframework/geometry/AttackHelicopterGeometry.cpp
        glframework/geometry/MeshBuilder.cpp
        glframework/geometry/Mesh.cpp
        glframework/scene/AttackHelicopterObject.cpp
        glframework/physics/HelicopterDynamics.cpp
    )
    target_include_directories(AttackHelicopterGeometryTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/glframework/geometry"
        "${CMAKE_CURRENT_SOURCE_DIR}/glframework/scene"
        "${CMAKE_CURRENT_SOURCE_DIR}/glframework/lighting" "${GLM_ROOT}")
    target_include_directories(AttackHelicopterGeometryTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/glframework/physics")
    target_link_libraries(AttackHelicopterGeometryTest PRIVATE glad)
    if(MSVC)
        target_compile_options(AttackHelicopterGeometryTest PRIVATE /utf-8)
        set_property(TARGET AttackHelicopterGeometryTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    endif()
    add_test(NAME AttackHelicopterGeometry COMMAND AttackHelicopterGeometryTest)
    add_executable(HelicopterDynamicsTest
        tests/HelicopterDynamicsTest.cpp
        glframework/physics/HelicopterDynamics.cpp
        glframework/Camera.cpp
        glframework/ThirdPersonCamera.cpp)
    target_include_directories(HelicopterDynamicsTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/glframework/physics"
        "${CMAKE_CURRENT_SOURCE_DIR}/glframework" "${GLM_ROOT}")
    if(MSVC)
        target_compile_options(HelicopterDynamicsTest PRIVATE /utf-8)
        set_property(TARGET HelicopterDynamicsTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    endif()
    add_test(NAME HelicopterDynamics COMMAND HelicopterDynamicsTest)
endif()

param(
    [Parameter(Mandatory = $false)]
    [string]$ProjectRoot = ".",

    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]$Configuration = "Debug",

    [string]$CMakePath = "C:/Program Files/CMake/bin/cmake.exe",

    [string]$Generator = "Visual Studio 17 2022"
)

# ASCII-only on purpose: Windows PowerShell 5.1 reads BOM-less scripts as ANSI,
# and non-ASCII comments can swallow the following newline (which used to turn
# the build invocation below into part of a comment).

$ErrorActionPreference = "Stop"

$resolvedRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$cmakeFile = Join-Path $resolvedRoot "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $cmakeFile)) {
    throw "CMakeLists.txt not found under: $resolvedRoot"
}
if (-not (Test-Path -LiteralPath $CMakePath)) {
    throw "CMake executable not found: $CMakePath"
}

$requiredFiles = @(
    "application/Application.cpp",
    "glframework/geometry/Mesh.cpp",
    "glframework/lighting/PBRRenderer.cpp",
    "asset/shader/pbr/pbr.vert",
    "asset/shader/pbr/pbr.frag"
)
foreach ($relativePath in $requiredFiles) {
    $candidate = Join-Path $resolvedRoot $relativePath
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Required project file is missing: $relativePath"
    }
}

$buildDirectory = Join-Path $resolvedRoot "build"
$executable = Join-Path $buildDirectory "$Configuration/RenderingCube.exe"
$started = Get-Date

& $CMakePath -S $resolvedRoot -B $buildDirectory -G $Generator -A x64
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

# /m:1 avoids several cl.exe instances writing the same PDB (C1090 / C1902).
& $CMakePath --build $buildDirectory --config $Configuration --clean-first -- /m:1
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE"
}

# A zero exit code alone is not proof: require a freshly produced executable.
if (-not (Test-Path -LiteralPath $executable)) {
    throw "Build produced no executable: $executable"
}
$artifact = Get-Item -LiteralPath $executable
if ($artifact.LastWriteTime -lt $started.AddSeconds(-5)) {
    throw "Executable was not rebuilt: $executable (last write $($artifact.LastWriteTime))"
}

Write-Host "RenderingCube verification build passed: $Configuration"
Write-Host "  executable: $executable"
Write-Host "  built at  : $($artifact.LastWriteTime)"
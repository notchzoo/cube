param(
    [Parameter(Mandatory = $false)]
    [string]$ProjectRoot = ".",

    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]$Configuration = "Debug",

    [string]$CMakePath = "C:/Program Files/CMake/bin/cmake.exe",

    [string]$Generator = "Visual Studio 17 2022",

    # Capture a deterministic screenshot and compare it against the local baseline.
    [switch]$Screenshot,

    # Force a clean build and require a freshly linked executable.
    [switch]$Clean,

    # Relative to ProjectRoot. Created on the first -Screenshot run.
    [string]$Baseline = "verify_out/baseline.bmp",

    [int]$Tolerance = 0
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
    "asset/shader/pbr/pbr.frag",
    "asset/shader/common/sky.glsl",
    "asset/shader/ui/overlay.vert",
    "asset/shader/ui/overlay.frag"
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

Push-Location $resolvedRoot
try {
    if (Test-Path -LiteralPath (Join-Path $resolvedRoot "CMakePresets.json")) {
        & $CMakePath --preset $(if ($Configuration -eq "Debug") { "debug" } else { "release" })
    }
    else {
        & $CMakePath -S $resolvedRoot -B $buildDirectory -G $Generator -A x64
    }
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    # /m:1 avoids several cl.exe instances writing the same PDB (C1090 / C1902).
    if ($Clean) {
        & $CMakePath --build $buildDirectory --config $Configuration --clean-first -- /m:1
    }
    else {
        & $CMakePath --build $buildDirectory --config $Configuration -- /m:1
    }
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }

    if (-not (Test-Path -LiteralPath $executable)) {
        throw "Build produced no executable: $executable"
    }
    $artifact = Get-Item -LiteralPath $executable
    if ($artifact.LastWriteTime -lt $started.AddSeconds(-5)) {
        if ($Clean) {
            throw "Executable was not rebuilt: $executable (last write $($artifact.LastWriteTime))"
        }
        Write-Host "Note: executable was already up to date (no relink needed)."
    }

    & ctest --test-dir $buildDirectory -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "ctest failed with exit code $LASTEXITCODE"
    }

    if ($Screenshot) {
        $shotDirectory = Join-Path $resolvedRoot "verify_out"
        New-Item -ItemType Directory -Force -Path $shotDirectory | Out-Null
        $candidateShot = Join-Path $shotDirectory "verify.bmp"
        $baselineShot = Join-Path $resolvedRoot $Baseline

        & $executable --screenshot $candidateShot --no-spin --spin-angle 28 --no-rotors --no-mouse
        if ($LASTEXITCODE -ne 0) {
            throw "Screenshot run failed with exit code $LASTEXITCODE"
        }

        if (-not (Test-Path -LiteralPath $baselineShot)) {
            Copy-Item -LiteralPath $candidateShot -Destination $baselineShot -Force
            Write-Host "Created baseline screenshot: $baselineShot"
        }
        else {
            $compare = Join-Path $resolvedRoot "tools/compare_screenshot.py"
            $python = Get-Command python -ErrorAction SilentlyContinue
            if ((Test-Path -LiteralPath $compare) -and $python) {
                & python $compare $baselineShot $candidateShot --tolerance $Tolerance
                if ($LASTEXITCODE -ne 0) {
                    throw "Screenshot regression: $candidateShot differs from $baselineShot (tolerance $Tolerance)"
                }
            }
            else {
                Write-Host "Skipping pixel comparison (python or tools/compare_screenshot.py missing)."
            }
        }
    }
}
finally {
    Pop-Location
}

Write-Host "RenderingCube verification passed: $Configuration"
Write-Host "  executable: $executable"
Write-Host "  built at  : $((Get-Item -LiteralPath $executable).LastWriteTime)"

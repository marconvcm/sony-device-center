# Windows dev helper: configure / build / test / run without remembering the
# MSVC + Qt environment dance. Usage:
#   .\scripts\win-dev.ps1 configure   # once, and after CMakeLists changes
#   .\scripts\win-dev.ps1 build       # incremental build
#   .\scripts\win-dev.ps1 test        # ctest with Qt DLLs on PATH
#   .\scripts\win-dev.ps1 run         # build, deploy Qt DLLs, launch the GUI
#   .\scripts\win-dev.ps1 ctl <args>  # build, run sonyctl with the given args
param(
    [Parameter(Position = 0)][ValidateSet('configure', 'build', 'test', 'run', 'ctl')]
    [string]$Command = 'build',
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Rest
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Qt = 'D:\Qt\6.10.0\msvc2022_64'
$VsRoot = 'C:\Program Files\Microsoft Visual Studio\2022\Community'
$Build = Join-Path $Root 'build'

# vcvars64.bat is the only reliable way to get cl/rc/link on PATH. Import the
# environment it produces into this PowerShell process.
if (-not $env:VSCMD_VER) {
    $vcvars = Join-Path $VsRoot 'VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($matches[1])" -Value $matches[2] }
    }
}
$env:PATH = "$Qt\bin;$VsRoot\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$VsRoot\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"

Set-Location $Root

function Invoke-Build {
    & cmake --build $Build --parallel
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
}

switch ($Command) {
    'configure' {
        & git submodule update --init --recursive
        & cmake -B $Build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$Qt" -DBUILD_TESTING=ON -DSONY_REQUIRE_QT=ON
    }
    'build' { Invoke-Build }
    'test' {
        & ctest --test-dir $Build --output-on-failure @Rest
    }
    'run' {
        Invoke-Build
        $exe = Join-Path $Build 'apps\device-center\sony-device-center.exe'
        & "$Qt\bin\windeployqt.exe" --release --no-translations --qmldir (Join-Path $Root 'apps\device-center\qml') $exe | Out-Null
        & $exe @Rest
    }
    'ctl' {
        Invoke-Build
        & (Join-Path $Build 'apps\sonyctl\sonyctl.exe') @Rest
    }
}

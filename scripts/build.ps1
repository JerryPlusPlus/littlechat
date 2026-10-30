param(
    [ValidateSet('mingw-release', 'x64-debug', 'x64-release')]
    [string]$Preset = 'mingw-release',
    [switch]$Package
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $projectRoot
try {
    if ($Preset -eq 'mingw-release') {
        $env:PATH = 'C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
    } else {
        if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
            $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            if (-not (Test-Path -LiteralPath $vswherePath)) {
                throw 'Run from a Visual Studio x64 developer shell, or install the VS C++ build tools.'
            }
            $vsRoot = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if (-not $vsRoot) { throw 'Visual Studio C++ tools were not found.' }
            $vsDevCmd = Join-Path $vsRoot.Trim() 'Common7/Tools/VsDevCmd.bat'
            $vsCommand = '"{0}" -arch=x64 -host_arch=x64 >nul && set' -f $vsDevCmd
            $compilerEnvironment = & $env:ComSpec /d /s /c $vsCommand
            if ($LASTEXITCODE -ne 0) { throw 'Unable to initialize the MSVC x64 environment.' }
            foreach ($entry in $compilerEnvironment) {
                if ($entry -match '^([^=]+)=(.*)$') {
                    [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
                }
            }
        }
        $env:PATH = 'C:\Qt\6.8.3\msvc2022_64\bin;' + $env:PATH
    }
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    ctest --test-dir "build/$Preset" --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    if ($Package) {
        $packagePath = Join-Path $projectRoot 'build/portable'
        cmake --install "build/$Preset" --prefix $packagePath
        if ($LASTEXITCODE -ne 0) { throw 'Deployment failed.' }
        Write-Host "Ready: $packagePath\bin\littlechat.exe"
    }
} finally {
    Pop-Location
}

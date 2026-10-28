param(
    [string]$VitaSdk = $env:VITASDK,
    [string]$CMake = 'cmake',
    [string]$BuildDirectory = 'build',
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
if (-not $VitaSdk -or -not (Test-Path -LiteralPath (Join-Path $VitaSdk 'share/vita.toolchain.cmake'))) {
    throw 'Pass -VitaSdk with the installed softfp VitaSDK directory, or set VITASDK.'
}
$env:VITASDK = (Resolve-Path -LiteralPath $VitaSdk).Path.Replace('\','/')
$env:PATH = (Join-Path $VitaSdk 'bin') + ';' + $env:PATH
& $CMake -S $PSScriptRoot -B (Join-Path $PSScriptRoot $BuildDirectory) -G 'Unix Makefiles' '-DCMAKE_BUILD_TYPE=Release'
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $CMake --build (Join-Path $PSScriptRoot $BuildDirectory) --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'Build failed; no new build should be installed.' }
Write-Host ('Built: ' + (Join-Path $PSScriptRoot "$BuildDirectory/Zombiewood.vpk"))

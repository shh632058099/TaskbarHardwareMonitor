$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$scriptPath = $MyInvocation.MyCommand.Path
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
$isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdministrator) {
    Write-Host 'Administrator rights are required to stop the running monitor and replace the DeskBand DLL.' -ForegroundColor Yellow
    Write-Host 'Requesting elevation...' -ForegroundColor Yellow
    $powershellExe = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
    if (-not (Test-Path -LiteralPath $powershellExe)) {
        throw "Windows PowerShell was not found: $powershellExe"
    }
    $escapedScriptPath = $scriptPath.Replace("'", "''")
    $elevatedCommand = "& '$escapedScriptPath'"
    Start-Process -FilePath $powershellExe -Verb RunAs -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-Command', $elevatedCommand
    )
    exit 0
}
$buildDir = Join-Path $projectRoot 'build-current'
$releaseDir = Join-Path $buildDir 'Release'
$monitorExe = Join-Path $releaseDir 'TaskbarHardwareMonitor.exe'
$bandDll = Join-Path $releaseDir 'TaskbarBand.dll'
$activatorExe = Join-Path $releaseDir 'TaskbarBandActivator.exe'
$testsExe = Join-Path $releaseDir 'TaskbarHardwareMonitorTests.exe'
$regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'
$explorerExe = Join-Path $env:WINDIR 'explorer.exe'
$bandClsid = '{65C3A923-7A8E-4A54-9E3A-22C8E25A9D01}'
$scriptLog = Join-Path $env:TEMP 'restart-taskbar-monitor.log'
$adminMarker = Join-Path $env:TEMP 'restart-taskbar-monitor-admin.started'
Set-Content -LiteralPath $adminMarker -Value (Get-Date).ToString('o') -Encoding ASCII
try {
    Start-Transcript -Path $scriptLog -Force | Out-Null
} catch {
    # Transcript is diagnostic only; do not block the restart flow.
}

function Resolve-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $fallback = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path -LiteralPath $fallback) {
        return $fallback
    }

    throw 'cmake.exe was not found. Install CMake or Visual Studio Build Tools with CMake support.'
}

function Stop-RequiredProcess([string]$name) {
    $processes = @(Get-Process -Name $name -ErrorAction SilentlyContinue)
    foreach ($process in $processes) {
        try {
            Stop-Process -Id $process.Id -Force -ErrorAction Stop
            $deadline = (Get-Date).AddSeconds(5)
            while ((Get-Process -Id $process.Id -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
                Start-Sleep -Milliseconds 100
            }
        } catch {
            throw "Could not stop $name PID $($process.Id). Run this script from an elevated PowerShell window."
        }
    }

    if (Get-Process -Name $name -ErrorAction SilentlyContinue) {
        throw "$name is still running. Build aborted to avoid using stale binaries."
    }
}

function Get-RegisteredBandDll {
    $key = "Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\$bandClsid\InprocServer32"
    if (-not (Test-Path -LiteralPath $key)) {
        return $null
    }
    return (Get-Item -LiteralPath $key).GetValue('')
}

function Unregister-Band([string]$dllPath) {
    if ([string]::IsNullOrWhiteSpace($dllPath) -or -not (Test-Path -LiteralPath $dllPath)) {
        return
    }
    Write-Host "Unregistering DeskBand: $dllPath" -ForegroundColor Yellow
    Start-Process -FilePath $regsvr32 -ArgumentList '/s', '/u', $dllPath -Wait
}

$cmake = Resolve-CMake

Write-Host 'Stopping Hardware Monitor before build...' -ForegroundColor Yellow
Stop-RequiredProcess 'TaskbarHardwareMonitor'

$registeredBandDll = Get-RegisteredBandDll
$runtimeConfigSource = $null
if (-not [string]::IsNullOrWhiteSpace($registeredBandDll)) {
    $registeredBandDir = Split-Path -Parent $registeredBandDll
    if (-not [string]::IsNullOrWhiteSpace($registeredBandDir)) {
        $candidateConfig = Join-Path $registeredBandDir 'config.json'
        if (Test-Path -LiteralPath $candidateConfig) {
            $runtimeConfigSource = $candidateConfig
        }
    }
}
Unregister-Band $registeredBandDll

Write-Host 'Stopping Explorer before build to release TaskbarBand.dll...' -ForegroundColor Yellow
Get-Process -Name explorer -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

# Explorer may auto-restart. Because the DeskBand has already been unregistered it should
# not reload TaskbarBand.dll, but stop it once more if Windows brought it back immediately.
if (Get-Process -Name explorer -ErrorAction SilentlyContinue) {
    Get-Process -Name explorer -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}

$bandLog = Join-Path $env:TEMP 'TaskbarBand-debug.log'
if (Test-Path -LiteralPath $bandLog) {
    Remove-Item -LiteralPath $bandLog -Force -ErrorAction SilentlyContinue
}

Write-Host "Configuring build directory: $buildDir" -ForegroundColor Yellow
& $cmake -S $projectRoot -B $buildDir
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code: $LASTEXITCODE"
}

Write-Host 'Building Release binaries...' -ForegroundColor Yellow
& $cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code: $LASTEXITCODE"
}

foreach ($artifact in @($monitorExe, $bandDll, $activatorExe)) {
    if (-not (Test-Path -LiteralPath $artifact)) {
        throw "Build artifact not found: $artifact"
    }
}

$targetConfig = Join-Path $releaseDir 'config.json'
if ($runtimeConfigSource) {
    $sourceFull = [System.IO.Path]::GetFullPath($runtimeConfigSource)
    $targetFull = [System.IO.Path]::GetFullPath($targetConfig)
    if (-not [string]::Equals($sourceFull, $targetFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host "Migrating runtime config: $runtimeConfigSource -> $targetConfig" -ForegroundColor Yellow
        Copy-Item -LiteralPath $runtimeConfigSource -Destination $targetConfig -Force
    }
}

if (Test-Path -LiteralPath $testsExe) {
    Write-Host 'Running native tests...' -ForegroundColor Yellow
    & $testsExe
    if ($LASTEXITCODE -ne 0) {
        throw "Native tests failed with exit code: $LASTEXITCODE"
    }
}

Write-Host "Registering newly built DeskBand: $bandDll" -ForegroundColor Yellow
Start-Process -FilePath $regsvr32 -ArgumentList '/s', $bandDll -Wait

Write-Host 'Starting Explorer...' -ForegroundColor Yellow
Start-Process -FilePath $explorerExe
Start-Sleep -Seconds 2

Write-Host "Starting newly built Hardware Monitor: $monitorExe" -ForegroundColor Yellow
Start-Process -FilePath $monitorExe -WorkingDirectory $releaseDir
Start-Sleep -Milliseconds 750

Write-Host 'Activating Hardware Monitor DeskBand...' -ForegroundColor Yellow
& $activatorExe
if ($LASTEXITCODE -ne 0) {
    throw "DeskBand activation failed with exit code: $LASTEXITCODE"
}
Start-Sleep -Seconds 1

$expectedBandPath = [System.IO.Path]::GetFullPath($bandDll)
$loadedBandPath = $null
$explorer = Get-Process -Name explorer -ErrorAction SilentlyContinue | Select-Object -First 1
if ($explorer) {
    try {
        $loadedBandPath = $explorer.Modules |
            Where-Object { $_.ModuleName -ieq 'TaskbarBand.dll' } |
            Select-Object -First 1 -ExpandProperty FileName
    } catch {
        Write-Warning 'Could not inspect Explorer modules. The DeskBand may still be loaded correctly.'
    }
}

if ($loadedBandPath) {
    $actualBandPath = [System.IO.Path]::GetFullPath($loadedBandPath)
    if (-not [string]::Equals($actualBandPath, $expectedBandPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Explorer loaded the wrong DeskBand DLL. Expected: $expectedBandPath ; Actual: $actualBandPath"
    }
    Write-Host "Verified Explorer loaded: $actualBandPath" -ForegroundColor Green
} else {
    Write-Warning 'Could not verify the loaded DeskBand DLL path from Explorer.'
}

Write-Host ''
Write-Host 'Done. Build, registration, restart, and DeskBand activation completed.' -ForegroundColor Green

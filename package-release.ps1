param(
    [string]$PackageName = 'TaskbarHardwareMonitor-portable'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $projectRoot 'build-package'
$releaseDir = Join-Path $buildDir 'Release'
$stageRoot = Join-Path $projectRoot '.package'
$stageDir = Join-Path $stageRoot 'TaskbarHardwareMonitor'
$zipPath = Join-Path $projectRoot ($PackageName + '.zip')

function Resolve-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $fallback = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path -LiteralPath $fallback) { return $fallback }

    throw 'cmake.exe was not found. Install CMake or Visual Studio Build Tools with CMake support.'
}

function Assert-File([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required package file not found: $path"
    }
}

$cmake = Resolve-CMake

Write-Host 'Configuring clean portable Release build...' -ForegroundColor Cyan
& $cmake -S $projectRoot -B $buildDir -A x64 -DBUILD_UNIT_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }

Write-Host 'Building Release binaries...' -ForegroundColor Cyan
& $cmake --build $buildDir --config Release --target TaskbarHardwareMonitor TaskbarBand TaskbarBandActivator TaskbarHardwareMonitorTests
if ($LASTEXITCODE -ne 0) { throw "Release build failed: $LASTEXITCODE" }

$testsExe = Join-Path $releaseDir 'TaskbarHardwareMonitorTests.exe'
Assert-File $testsExe
Write-Host 'Running native tests...' -ForegroundColor Cyan
& $testsExe
if ($LASTEXITCODE -ne 0) { throw "Native tests failed: $LASTEXITCODE" }

$monitorExe = Join-Path $releaseDir 'TaskbarHardwareMonitor.exe'
$bandDll = Join-Path $releaseDir 'TaskbarBand.dll'
$activatorExe = Join-Path $releaseDir 'TaskbarBandActivator.exe'
$launcher = Join-Path $projectRoot 'resources\package\start-monitor.cmd'
$readme = Join-Path $projectRoot 'README.md'
$license = Join-Path $projectRoot 'LICENSE'
$thirdPartyNotices = Join-Path $projectRoot 'THIRD-PARTY-NOTICES.md'
$lgplLicense = Join-Path $projectRoot 'LICENSES\LGPL-2.1.txt'
$pawnIoNotice = Join-Path $projectRoot 'LICENSES\PawnIO-NOTICE.md'
foreach ($file in @($monitorExe, $bandDll, $activatorExe, $launcher, $readme, $license, $thirdPartyNotices, $lgplLicense, $pawnIoNotice)) {
    Assert-File $file
}

if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

Copy-Item -LiteralPath $monitorExe -Destination $stageDir
Copy-Item -LiteralPath $bandDll -Destination $stageDir
Copy-Item -LiteralPath $activatorExe -Destination $stageDir
Copy-Item -LiteralPath $launcher -Destination $stageDir
Copy-Item -LiteralPath $readme -Destination (Join-Path $stageDir 'README.md')
Copy-Item -LiteralPath $license -Destination (Join-Path $stageDir 'LICENSE')
Copy-Item -LiteralPath $thirdPartyNotices -Destination (Join-Path $stageDir 'THIRD-PARTY-NOTICES.md')
$stageLicenses = Join-Path $stageDir 'LICENSES'
New-Item -ItemType Directory -Path $stageLicenses -Force | Out-Null
Copy-Item -LiteralPath $lgplLicense -Destination (Join-Path $stageLicenses 'LGPL-2.1.txt')
Copy-Item -LiteralPath $pawnIoNotice -Destination (Join-Path $stageLicenses 'PawnIO-NOTICE.md')

# Reuse current runtime preferences when available, but never ship an enabled
# startup task in a portable ZIP. The recipient can enable it from Settings.
$currentConfig = Join-Path $projectRoot 'build-current\Release\config.json'
$configDestination = Join-Path $stageDir 'config.json'
if (Test-Path -LiteralPath $currentConfig) {
    $config = Get-Content -LiteralPath $currentConfig -Raw | ConvertFrom-Json
    $config.start_with_windows = $false
    $config | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $configDestination -Encoding UTF8
} else {
    @'
{
  "refresh_interval": 1000,
  "taskbar": {
    "enabled": true,
    "display_mode": "full",
    "rows": 1,
    "show_cpu_temperature": true,
    "show_cpu_usage": true,
    "show_gpu_temperature": false,
    "show_disk_temperature": false,
    "show_network": true,
    "show_power": false,
    "show_memory": true,
    "show_gpu_usage": false,
    "show_vram": false,
    "show_disk_io": false,
    "show_cpu_clock": false,
    "show_gpu_power": false,
    "show_fan": false,
    "show_battery": false,
    "show_system_power": false,
    "value_color_custom": false,
    "value_color": 16119285,
    "font_name": "Segoe UI",
    "font_size": 10,
    "font_weight": 400,
    "format": "",
    "fixed_width": true,
    "tabular_numbers": true
  },
  "network_adapter": "auto",
  "storage_drive": -1,
  "start_with_windows": false
}
'@ | Set-Content -LiteralPath $configDestination -Encoding UTF8
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Write-Host "Creating portable ZIP: $zipPath" -ForegroundColor Cyan
Compress-Archive -Path $stageDir -DestinationPath $zipPath -CompressionLevel Optimal -Force
Assert-File $zipPath

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $entryNames = @($archive.Entries | ForEach-Object { $_.FullName.Replace('/', '\') })
    $requiredEntries = @(
        'TaskbarHardwareMonitor\TaskbarHardwareMonitor.exe',
        'TaskbarHardwareMonitor\TaskbarBand.dll',
        'TaskbarHardwareMonitor\TaskbarBandActivator.exe',
        'TaskbarHardwareMonitor\config.json',
        'TaskbarHardwareMonitor\start-monitor.cmd',
        'TaskbarHardwareMonitor\README.md',
        'TaskbarHardwareMonitor\LICENSE',
        'TaskbarHardwareMonitor\THIRD-PARTY-NOTICES.md',
        'TaskbarHardwareMonitor\LICENSES\LGPL-2.1.txt'
        'TaskbarHardwareMonitor\LICENSES\PawnIO-NOTICE.md'
    )
    foreach ($entry in $requiredEntries) {
        if ($entryNames -notcontains $entry) {
            throw "ZIP validation failed, missing entry: $entry"
        }
    }
} finally {
    $archive.Dispose()
}

$hash = Get-FileHash -LiteralPath $zipPath -Algorithm SHA256
$sizeMb = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 2)
Write-Host ''
Write-Host 'Portable package created successfully.' -ForegroundColor Green
Write-Host "ZIP:    $zipPath"
Write-Host "Size:   $sizeMb MB"
Write-Host "SHA256: $($hash.Hash)"
Write-Host 'Usage: extract the ZIP, then double-click start-monitor.cmd.'

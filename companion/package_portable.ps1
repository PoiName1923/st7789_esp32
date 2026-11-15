param(
    [string]$IdfRoot = 'C:\esp\v5.5.6\esp-idf',
    [string]$EspressifTools = 'C:\Espressif\tools'
)

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'dist'))
if (-not $output.StartsWith($repo + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Distribution target is outside this workspace.'
}
if (-not (Test-Path -LiteralPath (Join-Path $output 'DesktopRobotUSB.exe'))) {
    throw 'Build DesktopRobotUSB.exe with PyInstaller first.'
}

$firmware = Join-Path $output 'firmware'
$toolchain = Join-Path $output 'toolchain'
New-Item -ItemType Directory -Force -Path $firmware, $toolchain | Out-Null

# Explicit whitelist: never package sdkconfig, NVS secrets, builds, or user GIFs.
foreach ($name in @('CMakeLists.txt', 'partitions.csv', 'sdkconfig.defaults', 'dependencies.lock')) {
    Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $firmware -Force
}
foreach ($name in @('main', 'managed_components')) {
    Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $firmware -Recurse -Force
}
$componentTarget = Join-Path $firmware 'components'
New-Item -ItemType Directory -Force -Path $componentTarget | Out-Null
Get-ChildItem -LiteralPath (Join-Path $repo 'components') -Directory |
    Where-Object { $_.Name -ne 'web' } |
    Copy-Item -Destination $componentTarget -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $firmware 'assets/gifs') | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'assets/gifs/README.md') -Destination (Join-Path $firmware 'assets/gifs') -Force

$idfTarget = Join-Path $toolchain 'esp-idf'
New-Item -ItemType Directory -Force -Path $idfTarget | Out-Null
Get-ChildItem -LiteralPath $IdfRoot -Force | Where-Object { $_.Name -ne '.git' } |
    Copy-Item -Destination $idfTarget -Recurse -Force
$constraint = Join-Path $env:USERPROFILE '.espressif/espidf.constraints.v5.5.txt'
if (-not (Test-Path -LiteralPath $constraint)) { throw "Missing IDF constraints: $constraint" }
Copy-Item -LiteralPath $constraint -Destination $toolchain -Force

foreach ($relative in @(
    'xtensa-esp-elf/esp-14.2.0_20260121',
    'cmake/3.30.2',
    'ninja/1.12.1',
    'esp-rom-elfs/20260528'
)) {
    $source = Join-Path $EspressifTools $relative
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing tool: $source" }
    $target = Join-Path (Join-Path $toolchain 'tools') $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $EspressifTools 'git') -Destination (Join-Path $toolchain 'tools') -Recurse -Force

# Submodule .git pointers refer to the developer's checkout; omit them from the portable copy.
Get-ChildItem -LiteralPath $idfTarget -Recurse -Force -Filter '.git' | ForEach-Object {
    $resolved = [IO.Path]::GetFullPath($_.FullName)
    if (-not $resolved.StartsWith($idfTarget + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected Git metadata target: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

$pythonSource = Join-Path $EspressifTools 'python'
$pythonTarget = Join-Path $toolchain 'python'
New-Item -ItemType Directory -Force -Path $pythonTarget | Out-Null
foreach ($name in @('python.exe', 'pythonw.exe', 'python311.dll', 'python3.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'python311.zip')) {
    $source = Join-Path $pythonSource $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $pythonTarget -Force }
}
foreach ($name in @('DLLs', 'Lib')) {
    Copy-Item -LiteralPath (Join-Path $pythonSource $name) -Destination $pythonTarget -Recurse -Force
}
$site = Join-Path $pythonTarget 'Lib/site-packages'
New-Item -ItemType Directory -Force -Path $site | Out-Null
Get-ChildItem -LiteralPath (Join-Path $pythonSource 'v5.5.6/venv/Lib/site-packages') -Force |
    Copy-Item -Destination $site -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $pythonTarget 'Scripts') | Out-Null
Copy-Item -LiteralPath (Join-Path $pythonTarget 'python.exe') -Destination (Join-Path $pythonTarget 'Scripts/python.exe') -Force
Set-Content -LiteralPath (Join-Path $pythonTarget 'pyvenv.cfg') -Encoding UTF8 -Value @(
    "home = $pythonTarget",
    'include-system-site-packages = false',
    'version = 3.11.15',
    'portable-desktop-robot = true'
)

Write-Host "Portable distribution prepared at $output"

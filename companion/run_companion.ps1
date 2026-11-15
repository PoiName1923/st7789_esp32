param([string]$Port)

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pythonCandidates = @(
    (Join-Path $repo 'companion/dist/toolchain/python/Scripts/python.exe'),
    'C:\Espressif\tools\python\v5.5.6\venv\Scripts\python.exe',
    'python'
)

$python = $pythonCandidates | Where-Object {
    if ([IO.Path]::IsPathRooted($_)) { Test-Path -LiteralPath $_ }
    else { Get-Command $_ -ErrorAction SilentlyContinue }
} | Select-Object -First 1

if (-not $python) {
    throw 'Không tìm thấy Python. Hãy cài ESP-IDF 5.5 hoặc Python 3.11.'
}

$vendor = Join-Path $PSScriptRoot '.vendor'
if (Test-Path -LiteralPath $vendor) { $env:PYTHONPATH = $vendor }

$arguments = @(Join-Path $PSScriptRoot 'robot_companion.py')
if ($Port) { $arguments += @('--port', $Port) }
& $python @arguments
exit $LASTEXITCODE

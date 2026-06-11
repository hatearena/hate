$ErrorActionPreference = "Stop"

$msys2 = (Get-Command msys2 -ErrorAction Stop).Source
$mingwShim = (Get-Command mingw -ErrorAction Stop).Source

Write-Host "Installing dependencies..."
$deps = @(
    "make"
    "mingw-w64-x86_64-cmake"
    "mingw-w64-x86_64-gcc"
    "mingw-w64-x86_64-enet"
    "mingw-w64-x86_64-SDL2"
    "mingw-w64-x86_64-SDL2_image"
    "mingw-w64-x86_64-SDL2_mixer"
    "mingw-w64-x86_64-SDL2_ttf"
    "mingw-w64-x86_64-zlib"
)
$depString = $deps -join " "
& $msys2 -lc "set -e; pacman -S --needed --noconfirm $depString"
if ($LASTEXITCODE -ne 0) {
    throw "Dependency install failed"
}

$projectRoot = Split-Path -Parent $PSCommandPath
$msysRoot = $projectRoot.Replace('\','/') -replace '^([A-Za-z]):','/$1'

Write-Host "Configuring..."
& $mingwShim -lc "cd '$msysRoot' && cmake --preset msys2"
if ($LASTEXITCODE -ne 0) {
    throw "Configuration failed"
}

Write-Host "Building..."
& $mingwShim -lc "cd '$msysRoot' && cmake --build --preset msys2"
if ($LASTEXITCODE -ne 0) {
    throw "Build failed"
}

$client = Join-Path $projectRoot "content" "hate_client.exe"
$server = Join-Path $projectRoot "content" "hate_server.exe"
if (!(Test-Path $client)) {
    throw "Client exe not created at $client"
}
if (!(Test-Path $server)) {
    throw "Server exe not created at $server"
}

Write-Host ""
Write-Host "SUCCESS"
Write-Host "Client: $client"
Write-Host "Server: $server"

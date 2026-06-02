$ErrorActionPreference = "Stop"

$msys2 = (Get-Command msys2 -ErrorAction Stop).Source
$mingwShim = (Get-Command mingw -ErrorAction Stop).Source

Write-Host "Installing dependencies..."
$deps = @(
    "make"
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
$srcDir = Join-Path $projectRoot "src"
$msysPath = $srcDir.Replace('\','/')
$msysPath = $msysPath -replace '^([A-Za-z]):','/$1'

Write-Host "Building..."
& $mingwShim -lc "cd '$msysPath' && make"
if ($LASTEXITCODE -ne 0) {
    throw "Build failed"
}

$client = Join-Path $srcDir "hate_client.exe"
$server = Join-Path $srcDir "hate_server.exe"
if (!(Test-Path $client)) {
    throw "Client exe not created"
}
if (!(Test-Path $server)) {
    throw "Server exe not created"
}

Write-Host ""
Write-Host "SUCCESS"
Write-Host "Client: $client"
Write-Host "Server: $server"
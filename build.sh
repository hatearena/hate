#!/bin/bash
set -e

UNAME_S=$(uname -s)

if [ "$UNAME_S" = "Darwin" ]; then
    echo "Building for MacOS..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(sysctl -n hw.ncpu)
    echo "Build complete. .app bundle: build/HateArena-MacOS.app"
else
    echo "Building for Linux with AppImage..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_APPIMAGE=ON
    cmake --build build -j$(nproc)
    cmake --build build --target appimage
    echo "Build complete. AppImage: build/"
fi

#!/bin/bash
set -e

UNAME_S=$(uname -s)

mkdir -p build
cd build

if [ "$UNAME_S" = "Darwin" ]; then
    echo "Building for MacOS..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(sysctl -n hw.ncpu)
    echo "Build complete."
else
    echo "Building for Linux with AppImage..."
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_APPIMAGE=ON
    make -j$(nproc)
    make appimage
    echo "Build complete. AppImage is in build/"
fi

#!/bin/bash
set -e

show_usage() {
    echo "Usage: $0 [ -client | -server | -appimage | -appbundle ]"
    echo ""
    echo "    -client     Build the client binary"
    echo "    -brun       Build the client binary and run it"
    echo "    -server     Build the server binary"
    echo "    -appimage   Build client and create Linux AppImage"
    echo "    -appbundle  Build client and create MacOS .app bundle"
    exit 1
}

if [ $# -eq 0 ]; then
    show_usage
fi

UNAME_S=$(uname -s)

case "$1" in
    -client)
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_APPIMAGE=OFF -DBUILD_APPBUNDLE=OFF
        cmake --build build --target hate_client -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
        ;;
    -brun)
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_APPIMAGE=OFF -DBUILD_APPBUNDLE=OFF
        cmake --build build --target hate_client -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
        cd content
        ./hate_client
        ;;
    -server)
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_APPIMAGE=OFF -DBUILD_APPBUNDLE=OFF
        cmake --build build --target hate_server -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
        ;;
    -appimage)
        if [ "$UNAME_S" = "Darwin" ]; then
            echo "Error: -appimage is only supported on Linux" >&2
            exit 1
        fi
        echo "Building for Linux with AppImage..."
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_APPIMAGE=ON -DBUILD_APPBUNDLE=OFF
        cmake --build build -j$(nproc)
        cmake --build build --target appimage
        echo "Build complete. AppImage: build/"
        ;;
    -appbundle)
        if [ "$UNAME_S" != "Darwin" ]; then
            echo "Error: -appbundle is only supported on macOS" >&2
            exit 1
        fi
        echo "Building for MacOS..."
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_APPIMAGE=OFF -DBUILD_APPBUNDLE=ON
        cmake --build build -j$(sysctl -n hw.ncpu)
        cmake --build build --target appbundle
        echo "Build complete. .app bundle: build/HateArena-MacOS.app"
        ;;
    *)
        show_usage
        ;;
esac

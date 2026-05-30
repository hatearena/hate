#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

msg() {
    printf "\033[1;32m==>\033[0m %s\n" "$*"
}

err() {
    printf "\033[1;31mERROR:\033[0m %s\n" "$*" >&2
    exit 1
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || err "$1 is required"
}

install_linux() {
    if command -v apt-get >/dev/null 2>&1; then
        msg "Detected Debian/Ubuntu"

        sudo apt-get update

        sudo apt-get install -y \
            build-essential \
            make \
            gcc \
            g++ \
            libenet-dev \
            libsdl2-dev \
            libsdl2-image-dev \
            libsdl2-mixer-dev \
            libglu1-mesa-dev \
            libgl1-mesa-dev \
            zlib1g-dev

    elif command -v dnf >/dev/null 2>&1; then
        msg "Detected Fedora"

        sudo dnf install -y \
            gcc \
            gcc-c++ \
            make \
            enet-devel \
            SDL2-devel \
            SDL2_image-devel \
            SDL2_mixer-devel \
            mesa-libGL-devel \
            mesa-libGLU-devel \
            zlib-devel

    elif command -v pacman >/dev/null 2>&1; then
        msg "Detected Arch"

        sudo pacman -Sy --needed \
            base-devel \
            enet \
            sdl2 \
            sdl2_image \
            sdl2_mixer \
            glu \
            mesa \
            zlib

    elif command -v zypper >/dev/null 2>&1; then
        msg "Detected openSUSE"

        sudo zypper install -y \
            gcc \
            gcc-c++ \
            make \
            libenet-devel \
            libSDL2-devel \
            libSDL2_image-devel \
            libSDL2_mixer-devel \
            Mesa-libGL-devel \
            Mesa-libGLU-devel \
            zlib-devel

    else
        err "Unsupported Linux distribution. Install SDL2, SDL2_image, SDL2_mixer, ENet, OpenGL/GLU and zlib development packages manually."
    fi
}

install_macos() {
    msg "Detected macOS"

    require_cmd brew

    brew install \
        enet \
        sdl2 \
        sdl2_image \
        sdl2_mixer
}

install_freebsd() {
    msg "Detected FreeBSD"

    sudo pkg install -y \
        gmake \
        enet \
        sdl2 \
        sdl2_image \
        sdl2_mixer
}

install_openbsd() {
    msg "Detected OpenBSD"

    doas pkg_add \
        gmake \
        enet \
        sdl2 \
        sdl2-image \
        sdl2-mixer
}

install_netbsd() {
    msg "Detected NetBSD"

    sudo pkgin install \
        gmake \
        enet \
        SDL2 \
        SDL2_image \
        SDL2_mixer
}

build_project() {
    msg "Building HATE"

    cd "$ROOT"

    make

    msg "Build complete"
}

verify_build() {
    if [ -f "$ROOT/content/hate_client" ]; then
        msg "Client built successfully"
    fi

    if [ -f "$ROOT/content/hate_server" ]; then
        msg "Server built successfully"
    fi
}

case "$(uname -s)" in
    Linux)
        install_linux
        ;;
    Darwin)
        install_macos
        ;;
    FreeBSD)
        install_freebsd
        ;;
    OpenBSD)
        install_openbsd
        ;;
    NetBSD)
        install_netbsd
        ;;
    *)
        err "Unsupported operating system: $(uname -s)"
        ;;
esac

build_project
verify_build

msg "HATE installed successfully"

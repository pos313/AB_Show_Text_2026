#!/bin/bash

# Live Text System - Dependency Setup Script
# This script helps set up dependencies for the Live Text System

set -e

echo "Live Text System - Setting up dependencies..."

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "msys" ]]; then
    OS="windows"
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi

echo "Detected OS: $OS"

# Install system dependencies
case $OS in
    "linux")
        echo "Installing Ubuntu/Debian dependencies..."
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            libglfw3-dev \
            libfreetype6-dev \
            libglm-dev \
            pkg-config \
            git \
            wget \
            openjdk-11-jdk
        ;;
    "macos")
        echo "Installing macOS dependencies..."
        if ! command -v brew &> /dev/null; then
            echo "Please install Homebrew first: https://brew.sh/"
            exit 1
        fi
        brew install glfw freetype glm cmake pkg-config
        ;;
    "windows")
        echo "Windows setup requires manual dependency installation"
        echo "Please install:"
        echo "  - Visual Studio 2019+ with C++ support"
        echo "  - CMake"
        echo "  - vcpkg package manager"
        echo "  - Spout SDK"
        exit 0
        ;;
esac

# Download and build Aeron
echo "Setting up Aeron..."
AERON_DIR="third_party/aeron"
AERON_VERSION="1.44.0"

if [ ! -d "$AERON_DIR" ]; then
    mkdir -p third_party
    cd third_party

    echo "Downloading Aeron v$AERON_VERSION..."
    wget "https://github.com/aeron-io/aeron/archive/refs/tags/$AERON_VERSION.tar.gz"
    tar -xzf "$AERON_VERSION.tar.gz"
    mv "aeron-$AERON_VERSION" aeron
    rm "$AERON_VERSION.tar.gz"

    cd aeron
    mkdir -p build
    cd build

    echo "Building Aeron..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc 2>/dev/null || echo 4)

    if [[ "$OS" == "linux" ]]; then
        sudo make install
    elif [[ "$OS" == "macos" ]]; then
        sudo make install
    fi

    cd ../../..
else
    echo "Aeron already exists, skipping..."
fi

# Download ImGui
echo "Setting up ImGui..."
IMGUI_DIR="third_party/imgui"
IMGUI_VERSION="v1.90"

if [ ! -d "$IMGUI_DIR" ]; then
    mkdir -p third_party
    cd third_party

    echo "Downloading ImGui $IMGUI_VERSION..."
    wget "https://github.com/ocornut/imgui/archive/refs/tags/$IMGUI_VERSION.tar.gz"
    tar -xzf "$IMGUI_VERSION.tar.gz"
    mv "imgui-${IMGUI_VERSION#v}" imgui
    rm "$IMGUI_VERSION.tar.gz"

    cd ..
else
    echo "ImGui already exists, skipping..."
fi

# Download gl3w
echo "Setting up gl3w..."
GL3W_DIR="third_party/gl3w"

if [ ! -d "$GL3W_DIR" ]; then
    mkdir -p third_party
    cd third_party

    echo "Cloning gl3w..."
    git clone https://github.com/skaslev/gl3w.git

    cd gl3w
    python gl3w_gen.py

    cd ../..
else
    echo "gl3w already exists, skipping..."
fi

echo ""
echo "Dependencies setup complete!"
echo ""
echo "Next steps:"
echo "1. Create build directory: mkdir build && cd build"
echo "2. Configure with CMake: cmake .."
echo "3. Build: make -j4"
echo ""
echo "For Windows, please also install:"
echo "  - Spout SDK from https://spout.zeal.co/"
echo "  - Configure CMake to find dependencies"
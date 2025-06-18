#!/bin/bash

# Build script for C++ Video Recognizer
# This script automates the build process for the ultimateALPR video recognizer

set -e  # Exit on any error

echo "=== C++ Video Recognizer Build Script ==="

# Check if we're in the right directory
if [ ! -f "videorecognizer.cpp" ]; then
    echo "Error: videorecognizer.cpp not found. Please run this script from the videorecognizer directory."
    exit 1
fi

# Check for required tools
echo "Checking build tools..."

if ! command -v cmake &> /dev/null; then
    echo "Error: cmake not found. Please install cmake first."
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "Error: make not found. Please install make first."
    exit 1
fi

# Detect system
SYSTEM=$(uname -s)
echo "Detected system: $SYSTEM"

# Check for required libraries
echo "Checking required libraries..."

# Check OpenCV
if ! pkg-config --exists opencv4; then
    if ! pkg-config --exists opencv; then
        echo "Warning: OpenCV not found via pkg-config. Make sure it's installed."
    else
        echo "Found OpenCV via pkg-config"
    fi
else
    echo "Found OpenCV4 via pkg-config"
fi

# Check nlohmann/json
if ! pkg-config --exists nlohmann_json; then
    echo "Warning: nlohmann/json not found via pkg-config. Make sure it's installed."
else
    echo "Found nlohmann/json via pkg-config"
fi

# Check cxxopts
if ! pkg-config --exists cxxopts; then
    echo "Warning: cxxopts not found via pkg-config. Make sure it's installed."
else
    echo "Found cxxopts via pkg-config"
fi

# Create build directory
echo "Creating build directory..."
rm -rf build
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the application
echo "Building application..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Check if build was successful
if [ -f "videorecognizer" ]; then
    echo "=== Build successful! ==="
    echo "Executable created: $(pwd)/videorecognizer"
    echo ""
    echo "Usage example:"
    echo "./videorecognizer --video /path/to/your/video.mp4 --assets ../../../assets"
    echo ""
    echo "For more options, run:"
    echo "./videorecognizer --help"
else
    echo "Error: Build failed. Check the error messages above."
    exit 1
fi 
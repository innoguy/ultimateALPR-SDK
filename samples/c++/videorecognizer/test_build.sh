#!/bin/bash

# Test script for C++ Video Recognizer
# This script tests the build and shows usage examples

set -e

echo "=== C++ Video Recognizer Test Script ==="

# Check if executable exists
if [ ! -f "build/videorecognizer" ]; then
    echo "Error: videorecognizer executable not found. Please run build.sh first."
    exit 1
fi

echo "Found videorecognizer executable: build/videorecognizer"

# Test help
echo ""
echo "Testing help command..."
cd build
./videorecognizer --help

echo ""
echo "=== Build Test Successful! ==="
echo ""
echo "To use the application:"
echo ""
echo "1. Basic usage:"
echo "   ./videorecognizer --video /path/to/your/video.mp4"
echo ""
echo "2. With custom assets:"
echo "   ./videorecognizer --video /path/to/your/video.mp4 --assets ../../../assets"
echo ""
echo "3. With specific charset:"
echo "   ./videorecognizer --video /path/to/your/video.mp4 --charset latin"
echo ""
echo "4. Process only first 30 seconds:"
echo "   ./videorecognizer --video /path/to/your/video.mp4 --duration 30"
echo ""
echo "5. Enable additional features:"
echo "   ./videorecognizer --video /path/to/your/video.mp4 \\"
echo "       --klass_vcr_enabled true \\"
echo "       --klass_vmmr_enabled true \\"
echo "       --openvino_device GPU"
echo ""
echo "Note: Make sure you have a valid license token if required by the SDK." 
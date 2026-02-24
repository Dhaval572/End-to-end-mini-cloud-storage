#!/bin/bash

# Simple build script for E2Eye
set -e

echo "=== E2Eye Build Script ==="

# Clean build directory
echo "Cleaning build directory..."
rm -rf build
mkdir build

# Configure with CMake
echo "Configuring build..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Executables created:"
ls -la server client

echo ""
echo "To run the server:"
echo "  cd build && ./server"
echo ""
echo "To run the client:"
echo "  cd build && ./client"
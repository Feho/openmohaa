#!/bin/bash
# OpenMoHAA Bot AI Improvements - Environment Setup Script
# This script sets up a reproducible development environment

set -e  # Exit on error

echo "=== OpenMoHAA Bot AI Development Environment Setup ==="

# Check we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Must be run from OpenMoHAA root directory"
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d ".cmake" ]; then
    echo "Creating build directory..."
    mkdir .cmake
fi

# Configure CMake
echo "Configuring CMake..."
cd .cmake
cmake ../

# Build the project
echo "Building project..."
cmake --build . -j$(nproc) 2>&1 | tee build.log | tail -n 10

# Run tests
echo "Running tests..."
if ! ctest --output-on-failure; then
    echo "Warning: Some tests failed"
    exit 1
fi

echo ""
echo "=== Setup Complete ==="
echo "Build directory: .cmake/"
echo "To rebuild: cd .cmake && cmake --build ."
echo "To test: cd .cmake && ctest"
echo ""
echo "In-game testing:"
echo "1. Start game and enable cheats: set cheats 1; set thereisnomonkey 1"
echo "2. Load map: set g_gametype 2; map dm/mohdm6"
echo "3. Spawn bot: bot add"
echo ""

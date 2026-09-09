#!/bin/bash

set -e

echo ""
echo "========================================"
echo "        .mr Marathi Compiler"
echo "        Installation & Build"
echo "========================================"
echo ""

echo "Checking dependencies..."

if ! command -v g++ >/dev/null 2>&1; then
    echo "→ g++ not found. Installing build tools..."
    sudo apt update
    sudo apt install -y build-essential
else
    echo "✓ g++ found"
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "→ CMake not found. Installing CMake..."
    sudo apt update
    sudo apt install -y cmake
else
    echo "✓ CMake found"
fi

if ! command -v nasm >/dev/null 2>&1; then
    echo "→ NASM not found. Installing NASM..."
    sudo apt update
    sudo apt install -y nasm
else
    echo "✓ NASM found"
fi

echo ""
echo "Dependencies ready."
echo ""

# Remove previous build/cache if it exists
if [ -d "build" ]; then
    echo "→ Existing build directory found."
    echo "→ Removing previous build and CMake cache..."

    # Handle build files accidentally created with sudo
    sudo chown -R "$USER":"$(id -gn)" build

    rm -rf build

    echo "✓ Previous build removed"
fi

echo ""
echo "<=====> Building .mr Marathi Compiler <=====>"
echo ""

echo "Configuring .mr Marathi Compiler..."
cmake -S . -B build
echo "✓ Build system configured"

echo ""

echo "Compiling and linking .mr compiler..."
cmake --build build
echo "✓ .mr compiler built successfully"

echo ""
echo "========================================"
echo "        .mr compiler is ready!"
echo "========================================"
echo ""

echo "Compiler: ./build/compiler"
echo ""
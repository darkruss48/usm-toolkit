#!/bin/bash
set -e

mkdir -p build_linux
cd build_linux
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

echo "Build complete: build_linux/usm_toolkit"

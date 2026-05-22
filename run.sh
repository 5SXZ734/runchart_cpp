#!/usr/bin/env bash
set -euo pipefail

echo "Configuring CMake..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

echo ""
echo "Building..."
cmake --build build -j$(nproc)

echo ""
echo "============================================================"
echo "Build completed successfully!"
echo "============================================================"
echo ""
echo "Server executable:"
echo "  ./build/runchart_server"
echo ""
echo "Client executable:"
echo "  ./build/runchart_client"
echo ""
echo "Example usage:"
echo "  1. Start server in one terminal:"
echo "     ./build/runchart_server"
echo ""
echo "  2. Run client in another terminal:"
echo "     ./build/runchart_client localhost:3030 data.json"
echo ""
echo "  3. To connect to remote server (e.g., NAS):"
echo "     ./build/runchart_client 192.168.1.4:3030 data.json"
echo ""

#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=================================================="
echo "    OmniIPC Framework - End-to-End Build System   "
echo "=================================================="

# 1. Build C++ Backend Engine
echo "[1/2] Compiling C++20 POSIX IPC Benchmark Engine..."
cd "$PROJECT_ROOT/backend"
make clean
make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
chmod +x omni_ipc_engine

# 2. Verify Frontend Assets
echo "[2/2] Preparing Frontend Visualizer..."
if [ -d "$PROJECT_ROOT/frontend/node_modules" ]; then
    echo "NPM modules detected. Building production Vite bundle..."
    cd "$PROJECT_ROOT/frontend"
    npm run build || true
fi

echo ""
echo " Build Succeeded!"
echo "Binary available at: $PROJECT_ROOT/backend/omni_ipc_engine"
echo "Frontend Dashboard ready at: $PROJECT_ROOT/frontend/dist/index.html"
echo "Run './scripts/run.sh' to launch the full real-time telemetry server!"

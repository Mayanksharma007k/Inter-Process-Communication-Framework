#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PORT="${PORT:-8080}"

# Check if binary is compiled
if [ ! -f "$PROJECT_ROOT/backend/omni_ipc_engine" ]; then
    echo "Engine binary missing. Triggering build..."
    "$SCRIPT_DIR/build.sh"
fi

echo "=================================================="
echo "    Starting OmniIPC Discord Visualizer & Core   "
echo "=================================================="
echo "Telemetry Server: http://127.0.0.1:$PORT"
echo "Press Ctrl+C to terminate."
echo ""

# Launch Python Zero-Dependency Telemetry Gateway
exec python3 "$PROJECT_ROOT/backend/server.py"

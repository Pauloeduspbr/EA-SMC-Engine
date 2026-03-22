#!/bin/bash
# =============================================================================
# SMC Engine DLL — Build Script
# Cross-compile with MinGW for Windows x64 (MT5)
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DLL_DIR="$SCRIPT_DIR/dll"
BUILD_DIR="$DLL_DIR/build"
DLL_NAME="smc_engine.dll"

# MT5 deployment paths
MT5_LIB_DIR="$HOME/.wine/drive_c/Program Files/easyMarkets MetaTrader 5/MQL5/Libraries"
MT5_EA_DIR="$HOME/.wine/drive_c/Program Files/easyMarkets MetaTrader 5/MQL5/Experts/EA_SMC"
MT5_AGENT_LIB="$HOME/.wine/drive_c/Program Files/easyMarkets MetaTrader 5/Tester/Agent-127.0.0.1-3000/MQL5/Libraries"

echo "============================================"
echo "  SMC Engine DLL — Build"
echo "============================================"

# Check for MinGW
if ! command -v x86_64-w64-mingw32-g++ &>/dev/null; then
    echo "ERROR: x86_64-w64-mingw32-g++ not found"
    echo "Install: sudo apt install g++-mingw-w64-x86-64"
    exit 1
fi

# Clean and create build dir
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMake configure with MinGW toolchain
echo ""
echo "[1/3] Configuring with CMake (MinGW x64)..."
cmake "$DLL_DIR" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_BUILD_TYPE=Release \
    2>&1

# Build
echo ""
echo "[2/3] Building..."
cmake --build . -- -j$(nproc) 2>&1

# Check result
if [ -f "$DLL_NAME" ]; then
    echo ""
    echo "[3/3] Build SUCCESS: $BUILD_DIR/$DLL_NAME"
    ls -lh "$DLL_NAME"

    # Deploy to MT5 Libraries
    if [ -d "$MT5_LIB_DIR" ]; then
        cp "$DLL_NAME" "$MT5_LIB_DIR/"
        echo "Deployed to: $MT5_LIB_DIR/$DLL_NAME"
    else
        echo "MT5 Libraries dir not found: $MT5_LIB_DIR"
    fi

    # Copy to EA directory (Tester needs it here to transfer to Agent)
    if [ -d "$MT5_EA_DIR" ]; then
        cp "$DLL_NAME" "$MT5_EA_DIR/"
        echo "Deployed to EA: $MT5_EA_DIR/$DLL_NAME"
    fi

    # Deploy to Strategy Tester Agent
    if [ -d "$MT5_AGENT_LIB" ]; then
        cp "$DLL_NAME" "$MT5_AGENT_LIB/"
        echo "Deployed to Agent: $MT5_AGENT_LIB/$DLL_NAME"
    fi

    # Copy MQL5 EA file
    MQL5_SRC="$SCRIPT_DIR/mql5/EA_SMC.mq5"
    if [ -f "$MQL5_SRC" ] && [ -d "$MT5_EA_DIR" ]; then
        cp "$MQL5_SRC" "$MT5_EA_DIR/"
        echo "Deployed EA to: $MT5_EA_DIR/EA_SMC.mq5"
    fi
else
    echo ""
    echo "BUILD FAILED — no DLL produced"
    exit 1
fi

echo ""
echo "============================================"
echo "  Done!"
echo "============================================"

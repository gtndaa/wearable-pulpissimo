#!/bin/bash
# =============================================================================
# Build Script for RISC-V Board Test (NusaCore / PULPissimo)
# Target: libraries/app/gui_app/test/board
# =============================================================================

set -e

# Target workspace path: /home/vm/icdec
WORKSPACE_ROOT="/home/vm/icdec"

CHROOT_DIR="$WORKSPACE_ROOT/chroot"
PULP_RUNTIME_DIR="$WORKSPACE_ROOT/pulp-runtime"
BOARD_TEST_DIR="$WORKSPACE_ROOT/wearable-pulpissimo/libraries/app/gui_app/test/board/test_watch_face"

echo "================================================"
echo "    Building GUI App for RISC-V Board"
echo "================================================"

# 1. Masukkan chroot ke PATH
echo "[1/5] Setting up toolchain PATH..."
export PATH="$PATH:$CHROOT_DIR/bin"
echo "      CHROOT bin added to PATH"

# 2. Aktifkan konfigurasi pulpissimo & genesys2
echo "[2/5] Sourcing pulp-runtime configs..."
if [ -f "$PULP_RUNTIME_DIR/configs/pulpissimo.sh" ]; then
    . "$PULP_RUNTIME_DIR/configs/pulpissimo.sh"
else
    echo "ERROR: $PULP_RUNTIME_DIR/configs/pulpissimo.sh not found!"
    exit 1
fi

if [ -f "$PULP_RUNTIME_DIR/configs/fpgas/pulpissimo/genesys2.sh" ]; then
    . "$PULP_RUNTIME_DIR/configs/fpgas/pulpissimo/genesys2.sh"
else
    echo "ERROR: $PULP_RUNTIME_DIR/configs/fpgas/pulpissimo/genesys2.sh not found!"
    exit 1
fi

# 3. Ubah konfigurasi frekuensi (untuk board NusaCore)
echo "[3/5] Setting NusaCore frequency flags..."
export PULPRT_CONFIG_ASFLAGS="-DARCHI_FPGA_PER_FREQUENCY=10000000 -DARCHI_FPGA_SOC_FREQUENCY=10000000"
export PULPRT_CONFIG_CFLAGS="-DARCHI_FPGA_PER_FREQUENCY=10000000 -DARCHI_FPGA_SOC_FREQUENCY=10000000"
export PULPRT_CONFIG_CXXFLAGS="-DARCHI_FPGA_PER_FREQUENCY=10000000 -DARCHI_FPGA_SOC_FREQUENCY=10000000"

# 4. Pindah ke direktori test board & jalankan make
echo "[4/5] Building software in $BOARD_TEST_DIR..."
cd "$BOARD_TEST_DIR"

make clean
make

echo "[5/5] Build Complete!"
echo "================================================"
echo "  To run on FPGA board, execute:"
echo "    cd $BOARD_TEST_DIR"
echo "    make run platform=fpga"
echo "================================================"

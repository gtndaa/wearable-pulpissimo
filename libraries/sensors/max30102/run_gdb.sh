#!/bin/bash
# =============================================================================
# Run GDB & Connect to OpenOCD Server to Load Program to NusaCore Board
# Target: libraries/sensors/max30102
# =============================================================================

set -e

WORKSPACE_ROOT="/home/vm/icdec"
CHROOT_DIR="$WORKSPACE_ROOT/chroot"
BOARD_TEST_DIR="$WORKSPACE_ROOT/wearable-pulpissimo/libraries/sensors/max30102"
ELF_FILE="$BOARD_TEST_DIR/build/test_ppg_hr/test_ppg_hr"

echo "================================================"
echo "    Connecting GDB to OpenOCD & Loading App"
echo "================================================"

# 1. Masukkan chroot ke PATH
export PATH="$PATH:$CHROOT_DIR/bin"

if [ ! -f "$ELF_FILE" ]; then
    echo "ERROR: Compiled ELF file not found at $ELF_FILE"
    echo "Please run './build.sh' first!"
    exit 1
fi

# 2. Jalankan GDB, sambungkan ke localhost:3333, lalu load program
riscv32-unknown-elf-gdb "$ELF_FILE" \
  -ex "target extended-remote localhost:3333" \
  -ex "load"

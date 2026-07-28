#!/bin/bash
# =============================================================================
# Run GDB & Connect to OpenOCD Server to Load Program to NusaCore Board
# Target: libraries/app/gui_app/test/board
# =============================================================================

set -e

WORKSPACE_ROOT="/home/vm/icdec"
CHROOT_DIR="$WORKSPACE_ROOT/chroot"
BOARD_TEST_DIR="$WORKSPACE_ROOT/wearable-pulpissimo/libraries/app/gui_app/test/board/test_watch_face"
ELF_FILE="$BOARD_TEST_DIR/build/watch_face_test/watch_face_test"

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

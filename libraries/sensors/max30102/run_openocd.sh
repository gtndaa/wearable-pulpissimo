#!/bin/bash
# =============================================================================
# OpenOCD Server Runner (JTAG Over WiFi / ESP32 Bridge)
# =============================================================================

set -e

WORKSPACE_ROOT="/home/vm/icdec"
CHROOT_DIR="$WORKSPACE_ROOT/chroot"
PULP_RUNTIME_DIR="$WORKSPACE_ROOT/pulp-runtime"
OPENOCD_SCRIPTS_DIR="$PULP_RUNTIME_DIR/arduino/tools/openocd/scripts"

echo "================================================"
echo "    Starting OpenOCD Server (JTAG WiFi)"
echo "================================================"

# 1. Setup PATH to include chroot binaries (OpenOCD)
export PATH="$PATH:$CHROOT_DIR/bin"

# 2. Run OpenOCD with NusaCore ESP32-WiFi JTAG configs
echo "Connecting via OpenOCD to NusaCore board..."
openocd \
  -f "$OPENOCD_SCRIPTS_DIR/cmsis-dap-tcp1.cfg" \
  -f "$OPENOCD_SCRIPTS_DIR/soft_nusacore.cfg"

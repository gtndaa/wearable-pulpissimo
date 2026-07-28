#!/bin/bash

# ==============================================================
# build.sh - Build script for NTP Sync (PULPissimo)
# ==============================================================

# 1. Tambahkan chroot bin ke PATH
export PATH="$PATH:/home/vm/icdec/chroot/bin"

# 2. Load konfigurasi pulp-runtime untuk PULPissimo + Genesys2 FPGA
. /home/vm/icdec/pulp-runtime/configs/pulpissimo.sh
. /home/vm/icdec/pulp-runtime/configs/fpgas/pulpissimo/genesys2.sh

# 3. Set flags frekuensi FPGA (10 MHz)
export PULPRT_CONFIG_ASFLAGS="-DARCHI_FPGA_PER_FREQUENCY=10000000 -DARCHI_FPGA_SOC_FREQUENCY=10000000"
export PULPRT_CONFIG_CFLAGS="-DARCHI_FPGA_PER_FREQUENCY=10000000 -DARCHI_FPGA_SOC_FREQUENCY=10000000"
export PULPRT_CONFIG_CXXFLAGS="-DARCHI_FPGA_PER_FREQUENCY=10000000 -DARCHI_FPGA_SOC_FREQUENCY=10000000"

# 4. Build
make clean
make

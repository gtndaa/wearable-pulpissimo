# ============================================================================
# GUI Application Layer — Root Makefile
# ============================================================================
# This is a convenience Makefile that delegates to the actual test Makefiles.
#
# Linux PC Simulation:
#   make test       → Run unit tests (23 assertions)
#   make sim        → Run visual ASCII simulation
#
# Board RISC-V (PULPissimo):
#   cd test/board
#   source /path/to/pulp-runtime/configs/pulpissimo.sh
#   make clean all run platform=fpga
# ============================================================================

.PHONY: test sim clean

test:
	$(MAKE) -C test/sim test

sim:
	$(MAKE) -C test/sim sim

clean:
	$(MAKE) -C test/sim clean

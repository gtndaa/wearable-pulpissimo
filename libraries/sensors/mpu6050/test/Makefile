# ============================================================================
# ICDeC - MPU-6050 Sensor Test Makefile
#
# Build dan run aplikasi test MPU-6050 pada ICDeC PULPissimo FPGA board.
#
# Usage (Unit Test - host):
#   make unit        Build dan jalankan unit test (mock I2C, tanpa hardware)
#   make unit_build  Hanya build unit test
#   make unit_run    Hanya jalankan unit test
#
# Usage (FPGA Test):
#   make all          Build test untuk FPGA
#   make run platform=fpga  Run test pada FPGA
#   make clean        Bersihkan build artifacts
# ============================================================================

SENSOR      = mpu6050
PULP_APP    = test_mpu6050
PULP_LDFLAGS += -Wl,--start-group -lc -Wl,--end-group
PULP_APP_C_SRCS = test_mpu6050.c ../mpu6050.c

# Compiler flags
PULP_CFLAGS = -O3 -g -I..

# Include pulp-runtime build rules
include $(PULPRT_HOME)/rules/pulp.mk

# ============================================================================
# Unit Test (host-side, mock I2C)
# ============================================================================

UNIT_TEST_BIN = unit_test_mpu6050
UNIT_TEST_SRC = unit_test_mpu6050.c

RM ?= rm -f

unit: unit_build unit_run

unit_build:
	$(CC) -o $(UNIT_TEST_BIN) $(UNIT_TEST_SRC) \
		-I.. -Imock -lm
	@echo "Build selesai: ./$(UNIT_TEST_BIN)"

unit_run:
	./$(UNIT_TEST_BIN)

unit_clean:
	$(RM) $(UNIT_TEST_BIN)

.PHONY: unit unit_build unit_run unit_clean

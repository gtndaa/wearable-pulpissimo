# ============================================================================
# ICDeC - Activity Classifier Hardware Test Makefile
#
# Build dan run test classifier pada ICDeC PULPissimo FPGA board
# menggunakan data ASLI dari sensor MPU-6050.
#
# Usage (FPGA Test):
#   make all                    Build test untuk FPGA
#   make run platform=fpga      Run test pada FPGA
#   make clean                  Bersihkan build artifacts
#
# Usage (Host Unit Test — data mock, tanpa hardware):
#   make unit                   Build dan jalankan unit test
#   make unit_clean             Bersihkan binary unit test
# ============================================================================

# --- PULP application configuration ---
PULP_APP = test_classifier_hw

# Source files:
#   - test_classifier_hw.c : test utama (FPGA, sensor asli)
#   - classifier.c         : modul classifier (sensor-agnostic)
#   - mpu6050.c            : driver sensor MPU-6050
PULP_APP_C_SRCS = test_classifier_hw.c \
                  ../classifier.c \
                  ../../sensors/mpu6050/mpu6050.c

# Linker flags (wajib untuk newlib pada PULPissimo)
PULP_LDFLAGS += -Wl,--start-group -lc -Wl,--end-group

# Compiler flags:
#   -I..                         : classifier headers (classifier.h, imu_types.h, dll)
#   -I../../sensors/mpu6050      : mpu6050 driver (mpu6050.h, mpu6050_adapter.h)
PULP_CFLAGS = -O3 -g \
              -I.. \
              -I../../sensors/mpu6050

# Include pulp-runtime build rules (hanya jika PULPRT_HOME di-set)
# Ini memungkinkan target 'unit' berjalan di host TANPA PULP SDK.
ifneq ($(PULPRT_HOME),)
include $(PULPRT_HOME)/rules/pulp.mk
endif

# ============================================================================
# Host Unit Test (mock data, tanpa hardware)
# ============================================================================

UNIT_TEST_BIN = test_classifier
UNIT_TEST_SRC = test_classifier.c ../classifier.c

RM ?= rm -f

unit: unit_build unit_run

unit_build:
	gcc -Wall -Wextra -Wpedantic -std=c99 -I.. \
		-o $(UNIT_TEST_BIN) $(UNIT_TEST_SRC)
	@echo "Build selesai: ./$(UNIT_TEST_BIN)"

unit_run:
	./$(UNIT_TEST_BIN)

unit_clean:
	$(RM) $(UNIT_TEST_BIN)

.PHONY: unit unit_build unit_run unit_clean

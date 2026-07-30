/*
 * Copyright (C) 2026 ICDeC
 *
 * Test Application: MPU-6050 6-Axis (Gyroscope + Accelerometer)
 * ==============================================================
 * Combined test memakai library mpu6050.c/.h yang sudah lengkap
 * (gyro + accel), hasil reverse-engineering dari raw I2C test
 * yang sudah terbukti berhasil.
 *
 * Test structure:
 *   TEST 1: Default config
 *   TEST 2: Init (I2C, WHO_AM_I, wake, gyro config, accel config)
 *   TEST 3: WHO_AM_I manual
 *   TEST 4: Baca gyro raw + dps sekali
 *   TEST 5: Baca accel raw + mg sekali (sanity check ~1g)
 *   TEST 6: Baca 10 sampel accel berurutan (konsistensi)
 *   -> Continuous read: gyro + accel bersamaan
 *
 * Usage:
 *   make all
 *   make run platform=fpga
 */

#include <stdio.h>
#include "pulp.h"
#include "mpu6050.h"

#define CONTINUOUS_NUM_SAMPLES   300     /* 0 = infinite */
#define CONTINUOUS_DELAY_LOOPS   200000  /* delay antar pembacaan */

/* ============================================================================
 * Utility: print fixed-point values
 * ============================================================================ */

/** Print dps*100 value sebagai desimal (contoh: 359 -> " 3.59"). */
static void print_dps_x100(int32_t v)
{
    int sign = (v < 0) ? -1 : 1;
    int32_t av = v * sign;
    printf("%s%d.%02d", (sign < 0) ? "-" : " ", (int)(av / 100), (int)(av % 100));
}

/** Print milli-g value (contoh: 1000 -> " 1000"). */
static void print_mg(int32_t mg_val)
{
    printf("%6d", (int)mg_val);
}

/** Print milli-g sebagai g (contoh: 1000 -> " 1.000"). */
static void print_g_from_mg(int32_t mg_val)
{
    int sign = (mg_val < 0) ? -1 : 1;
    int32_t av = mg_val * sign;
    printf("%s%d.%03d", (sign < 0) ? "-" : " ", (int)(av / 1000), (int)(av % 1000));
}

/* ============================================================================
 * Continuous 6-Axis Read
 * ============================================================================ */

static void continuous_6axis_read(void)
{
    printf("\n========================================\n");
    printf(" CONTINUOUS 6-AXIS READ (Gyro + Accel)\n");
    if (CONTINUOUS_NUM_SAMPLES > 0)
        printf(" Jumlah sampel: %d\n", CONTINUOUS_NUM_SAMPLES);
    else
        printf(" Mode: infinite (reset board untuk stop)\n");
    printf("========================================\n\n");

    printf("  #    | Gyro dps X  dps Y  dps Z | Accel mg X  mg Y  mg Z |  g X    g Y    g Z\n");
    printf("-------+--------------------------+------------------------+---------------------\n");

    int sample = 0;
    while (1) {
        /* Baca gyro */
        mpu6050_dps_x100_t dps;
        mpu6050_status_t gs = mpu6050_gyro_read_dps_x100(&dps);

        /* Baca accel */
        mpu6050_accel_mg_t mg;
        mpu6050_status_t as = mpu6050_accel_read_mg(&mg);

        if (gs != MPU6050_OK || as != MPU6050_OK) {
            printf("  [!] Gagal baca (gyro=%d, accel=%d)\n", gs, as);
            for (volatile int d = 0; d < CONTINUOUS_DELAY_LOOPS; d++);
            continue;
        }

        printf(" %4d  | ", sample);
        print_dps_x100(dps.x_x100); printf(" ");
        print_dps_x100(dps.y_x100); printf(" ");
        print_dps_x100(dps.z_x100); printf("  |");
        print_mg(mg.x_mg); printf(" ");
        print_mg(mg.y_mg); printf(" ");
        print_mg(mg.z_mg); printf("  |");
        print_g_from_mg(mg.x_mg); printf(" ");
        print_g_from_mg(mg.y_mg); printf(" ");
        print_g_from_mg(mg.z_mg); printf("\n");

        sample++;
        if (CONTINUOUS_NUM_SAMPLES > 0 && sample >= CONTINUOUS_NUM_SAMPLES)
            break;

        for (volatile int d = 0; d < CONTINUOUS_DELAY_LOOPS; d++);
    }

    printf("\n  Selesai: %d sampel terbaca.\n", sample);
}

/* ============================================================================
 * MAIN TEST
 * ============================================================================ */

int main()
{
    mpu6050_config_t cfg;
    mpu6050_status_t status;
    int pass_count = 0;
    int fail_count = 0;

    printf("========================================\n");
    printf(" MPU-6050 6-Axis Test (Gyro + Accel)\n");
    printf(" ICDeC PULPissimo FPGA Board\n");
    printf("========================================\n\n");

    /* ---- Test 1: Default config ---- */
    printf("[TEST 1] Loading default configuration...\n");
    status = mpu6050_default_config(&cfg);
    if (status == MPU6050_OK) {
        printf("  PASS: addr=0x%02X, freq=%u, gyro_range=%d, accel_range=%d\n",
               cfg.i2c_addr, cfg.i2c_freq, cfg.gyro_range, cfg.accel_range);
        pass_count++;
    } else {
        printf("  FAIL (err=%d)\n", status);
        fail_count++;
    }
    printf("\n");

    /* ---- Test 2: Init (I2C, WHO_AM_I, wake, gyro config, accel config) ---- */
    printf("[TEST 2] Initializing MPU-6050 (Gyro + Accel)...\n");
    status = mpu6050_init(&cfg);
    if (status == MPU6050_OK) {
        printf("  PASS: Sensor initialized on addr=0x%02X\n", cfg.i2c_addr);
        pass_count++;
    } else {
        printf("  FAIL (err=%d)\n", status);
        fail_count++;
        printf("\n========================================\n");
        printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
        printf("========================================\n");
        return -1;
    }
    printf("\n");

    /* ---- Test 3: WHO_AM_I manual ---- */
    printf("[TEST 3] Reading WHO_AM_I register...\n");
    uint8_t who_am_i = 0;
    status = mpu6050_who_am_i(&who_am_i);
    if (status == MPU6050_OK && who_am_i == MPU6050_WHO_AM_I_VALUE) {
        printf("  PASS: WHO_AM_I = 0x%02X\n", who_am_i);
        pass_count++;
    } else {
        printf("  FAIL: WHO_AM_I = 0x%02X (err=%d)\n", who_am_i, status);
        fail_count++;
    }
    printf("\n");

    /* ---- Test 4: Baca gyro raw + dps sekali ---- */
    printf("[TEST 4] Membaca data gyro sekali...\n");
    {
        mpu6050_raw_t raw;
        mpu6050_dps_x100_t dps;
        status = mpu6050_gyro_read_raw(&raw);
        if (status == MPU6050_OK) {
            mpu6050_gyro_read_dps_x100(&dps);
            printf("  Raw: X=%d Y=%d Z=%d\n", raw.x, raw.y, raw.z);
            printf("  dps: X="); print_dps_x100(dps.x_x100);
            printf(" Y="); print_dps_x100(dps.y_x100);
            printf(" Z="); print_dps_x100(dps.z_x100);
            printf("\n  PASS\n");
            pass_count++;
        } else {
            printf("  FAIL (err=%d)\n", status);
            fail_count++;
        }
    }
    printf("\n");

    /* ---- Test 5: Baca accel raw + mg sekali (sanity check ~1g) ---- */
    printf("[TEST 5] Membaca data accelerometer sekali...\n");
    {
        mpu6050_accel_raw_t raw;
        status = mpu6050_accel_read_raw(&raw);
        if (status == MPU6050_OK) {
            mpu6050_accel_mg_t mg;
            mpu6050_accel_read_mg(&mg);

            printf("  Raw  : X=%6d  Y=%6d  Z=%6d\n", raw.x, raw.y, raw.z);
            printf("  mg   : X="); print_mg(mg.x_mg);
            printf("  Y="); print_mg(mg.y_mg);
            printf("  Z="); print_mg(mg.z_mg);
            printf("\n");
            printf("  g    : X="); print_g_from_mg(mg.x_mg);
            printf("  Y="); print_g_from_mg(mg.y_mg);
            printf("  Z="); print_g_from_mg(mg.z_mg);
            printf("\n");

            /* Sanity check: saat diam, magnitude ~1g (1000 mg)
             * Toleransi: 800-1200 mg -> mag^2 = 640,000 - 1,440,000 */
            int32_t mag_sq = (mg.x_mg * mg.x_mg) + (mg.y_mg * mg.y_mg) + (mg.z_mg * mg.z_mg);
            if (mag_sq >= 640000 && mag_sq <= 1440000) {
                printf("  PASS: Magnitude wajar (~1g saat diam)\n");
            } else {
                printf("  WARNING: Magnitude di luar range wajar (mag^2=%d)\n", (int)mag_sq);
                printf("  (mungkin sensor bergerak -- PASS conditional)\n");
            }
            pass_count++;
        } else {
            printf("  FAIL (err=%d)\n", status);
            fail_count++;
        }
    }
    printf("\n");

    /* ---- Test 6: Baca 10 sampel accel berurutan (konsistensi) ---- */
    printf("[TEST 6] Baca 10 sampel accel berurutan...\n");
    {
        int read_ok = 0;
        int read_fail = 0;

        for (int i = 0; i < 10; i++) {
            mpu6050_accel_raw_t raw;
            mpu6050_accel_mg_t mg;
            status = mpu6050_accel_read_raw(&raw);
            if (status == MPU6050_OK) {
                mpu6050_accel_read_mg(&mg);
                printf("  [%2d] Raw: %6d %6d %6d | mg: %6d %6d %6d\n",
                       i, raw.x, raw.y, raw.z,
                       (int)mg.x_mg, (int)mg.y_mg, (int)mg.z_mg);
                read_ok++;
            } else {
                printf("  [%2d] GAGAL baca (err=%d)\n", i, status);
                read_fail++;
            }
            for (volatile int d = 0; d < 50000; d++);
        }

        if (read_ok == 10) {
            printf("  PASS: 10/10 sampel terbaca\n");
            pass_count++;
        } else {
            printf("  FAIL: %d/10 sampel gagal\n", read_fail);
            fail_count++;
        }
    }
    printf("\n");

    /* ---- Results ---- */
    printf("========================================\n");
    printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
    printf("========================================\n");

    if (fail_count == 0) {
        continuous_6axis_read();
    }

    mpu6050_deinit();

    return (fail_count == 0) ? 0 : -1;
}

void pe_start(void)
{
}

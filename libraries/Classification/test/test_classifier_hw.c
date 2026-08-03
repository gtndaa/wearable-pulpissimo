/* ============================================================================
 * ICDeC Activity Classifier
 * File   : test/test_classifier_hw.c
 * Module : Hardware Test (PULPissimo FPGA + MPU-6050)
 * Copyright (C) 2026 ICDeC
 * ============================================================================
 *
 * Test ini menjalankan pipeline classifier LENGKAP di hardware asli:
 *   1. Inisialisasi MPU-6050 (I2C, WHO_AM_I, wake, config — via mpu6050_init())
 *   2. Baca data 6-axis (accel + gyro) melalui adapter (satu panggilan)
 *   3. Feed ke classifier secara real-time
 *   4. Tampilkan hasil klasifikasi + fitur per window
 *
 * BERBEDA dari test_classifier.c (host mock):
 *   - Pakai data ASLI dari sensor MPU-6050
 *   - Dikompilasi dengan PULP toolchain (bukan host GCC)
 *   - Butuh hardware (FPGA board + sensor terpasang)
 *   - Memakai mpu6050 library + adapter
 *
 * PRINSIP TETAP DIPERTAHANKAN:
 *   - classifier.c/h TIDAK include header sensor — adapter yang menjembatani
 *   - Test ini yang meng-include KEDUANYA (mpu6050 driver + classifier)
 *
 * Usage:
 *   make all
 *   make run platform=fpga
 * ============================================================================ */

#include <stdio.h>
#include "pulp.h"

/* --- Sensor driver + adapter --- */
#include "mpu6050.h"
#include "mpu6050_adapter.h"

/* --- Classifier (sensor-agnostic) --- */
#include "classifier.h"
#include "classifier_config.h"

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

/** Jumlah window yang akan diklasifikasi dalam mode continuous.
 *  Setiap window = CLF_WINDOW_SIZE sampel.
 *  Total sampel = NUM_WINDOWS × CLF_WINDOW_SIZE.
 *  0 = infinite (reset board untuk stop). */
#define NUM_WINDOWS             20

/** Delay antar pembacaan sampel (dalam loop iterasi).
 *  Mengatur efektif sampling rate.
 *  200000 loops ≈ ~20ms pada PULPissimo → ~50 Hz effective rate. */
#define SAMPLE_DELAY_LOOPS      200000

/** Jika 1, tampilkan raw data setiap sampel (verbose).
 *  Jika 0, hanya tampilkan hasil klasifikasi per window (ringkas). */
#define VERBOSE_RAW             0

/** Jika 1, tampilkan detail fitur setiap window.
 *  Berguna untuk tuning threshold. */
#define SHOW_FEATURES           1

/* ============================================================================
 * Helper: nama aktivitas untuk output readable
 * ============================================================================ */

static const char *activity_name(activity_t act)
{
    switch (act) {
        case ACTIVITY_UNKNOWN: return "UNKNOWN";
        case ACTIVITY_STILL:   return "STILL  ";
        case ACTIVITY_SIT:     return "SIT    ";
        case ACTIVITY_STAND:   return "STAND  ";
        case ACTIVITY_LIE:     return "LIE    ";
        case ACTIVITY_WALK:    return "WALK   ";
        case ACTIVITY_RUN:     return "RUN    ";
        case ACTIVITY_FALL:    return "FALL   ";
        default:               return "???    ";
    }
}

/** Nama status mpu6050 untuk pesan error yang informatif. */
static const char *mpu6050_status_name(mpu6050_status_t st)
{
    switch (st) {
        case MPU6050_OK:          return "OK";
        case MPU6050_ERR_I2C:     return "ERR_I2C";
        case MPU6050_ERR_ID:      return "ERR_ID";
        case MPU6050_ERR_CONFIG:  return "ERR_CONFIG";
        case MPU6050_ERR_TIMEOUT: return "ERR_TIMEOUT";
        case MPU6050_ERR_NULL:    return "ERR_NULL";
        default:                  return "???";
    }
}

/* ============================================================================
 * Delay sederhana (busy-wait, tanpa timer HW)
 * ============================================================================ */

static void simple_delay(int loops)
{
    volatile int i;
    for (i = 0; i < loops; i++) {
        __asm__ volatile ("nop");
    }
}

/* ============================================================================
 * Inisialisasi MPU-6050
 *
 * mpu6050_init() SUDAH mencakup: buka I2C, coba alamat 0x69 lalu 0x68,
 * verifikasi WHO_AM_I, wake dari sleep, dan konfigurasi GYRO_CONFIG/
 * ACCEL_CONFIG/DLPF/SMPLRT_DIV — jadi tidak perlu panggilan terpisah
 * untuk wake atau config default.
 * ============================================================================ */

static int init_sensor(void)
{
    mpu6050_config_t cfg;
    mpu6050_status_t status;
    uint8_t who_am_i = 0;

    printf("Inisialisasi MPU-6050...\n");

    status = mpu6050_default_config(&cfg);
    if (status != MPU6050_OK) {
        printf("ERROR: mpu6050_default_config() gagal (%s)\n",
               mpu6050_status_name(status));
        return -1;
    }

    status = mpu6050_init(&cfg);
    if (status != MPU6050_OK) {
        printf("ERROR: mpu6050_init() gagal (%s)\n",
               mpu6050_status_name(status));
        return -1;
    }

    status = mpu6050_who_am_i(&who_am_i);
    if (status != MPU6050_OK) {
        printf("ERROR: mpu6050_who_am_i() gagal (%s)\n",
               mpu6050_status_name(status));
        return -1;
    }

    printf("  WHO_AM_I = 0x%02X (expected 0x%02X)\n",
           who_am_i, MPU6050_WHO_AM_I_VALUE);
    if (who_am_i != MPU6050_WHO_AM_I_VALUE) {
        printf("ERROR: WHO_AM_I mismatch, sensor tidak terdeteksi!\n");
        return -1;
    }

    printf("MPU-6050 siap (I2C addr=0x%02X).\n\n", cfg.i2c_addr);
    return 0;
}

/* ============================================================================
 * Baca satu sampel dari sensor via adapter
 *
 * mpu6050_to_imu_sample() melakukan KEDUA pembacaan (accel + gyro) dan
 * langsung mengisi imu_sample_t — tidak perlu struct raw perantara.
 * ============================================================================ */

static int read_sample(imu_sample_t *sample)
{
    mpu6050_status_t status = mpu6050_to_imu_sample(sample);

    if (status != MPU6050_OK) {
        printf("  [WARN] Baca sampel gagal (%s)\n",
               mpu6050_status_name(status));
        return -1;
    }
    return 0;
}

/* ============================================================================
 * Tampilkan fitur untuk debugging/tuning
 * ============================================================================ */

static void print_features(const imu_sample_t *buf, int n)
{
    accel_features_t af;
    gyro_features_t gf;

    extract_features_accel(buf, n, &af);
    extract_features_gyro(buf, n, &gf);

    printf("    [Fitur] SMA=%ld SVM_max=%lu tilt=%ld | "
           "energy=%ld zcr=%ld mean_abs=%ld\n",
           (long)af.sma, (unsigned long)af.svm_max, (long)af.tilt_ratio,
           (long)gf.energy, (long)gf.zcr, (long)gf.mean_abs);
}

/* ============================================================================
 * Mode: Single window classification (verbose, untuk debugging)
 * ============================================================================ */

static void run_single_window_test(void)
{
    printf("=== Mode: Single Window Test ===\n\n");

    classifier_init();
    imu_sample_t window_copy[CLF_WINDOW_SIZE]; /* untuk print_features setelahnya */
    int idx = 0;

    printf("Mengumpulkan %d sampel...\n", CLF_WINDOW_SIZE);

    activity_t result = ACTIVITY_UNKNOWN;
    while (result == ACTIVITY_UNKNOWN) {
        imu_sample_t s;
        if (read_sample(&s) != 0) {
            printf("ERROR: gagal baca sampel dari sensor!\n");
            return;
        }

        if (idx < CLF_WINDOW_SIZE) {
            window_copy[idx++] = s;
        }

        if (VERBOSE_RAW) {
            printf("  ax=%6d ay=%6d az=%6d gx=%6d gy=%6d gz=%6d\n",
                   s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
        }

        result = classifier_update(&s);
        simple_delay(SAMPLE_DELAY_LOOPS);
    }

    printf("  Klasifikasi: %s (kode=%d)\n\n", activity_name(result), result);

    if (SHOW_FEATURES) {
        print_features(window_copy, idx);
    }
}

/* ============================================================================
 * Mode: Continuous classification (real-time monitoring)
 * ============================================================================ */

static void run_continuous_test(void)
{
    printf("=== Mode: Continuous Classification ===\n\n");

    classifier_init();

    imu_sample_t window_copy[CLF_WINDOW_SIZE];
    int idx = 0;
    int window_count = 0;
    long sample_count = 0;

    while (NUM_WINDOWS == 0 || window_count < NUM_WINDOWS) {
        imu_sample_t s;
        if (read_sample(&s) != 0) {
            printf("ERROR: gagal baca sampel dari sensor!\n");
            return;
        }

        if (idx < CLF_WINDOW_SIZE) {
            window_copy[idx++] = s;
        }

        if (VERBOSE_RAW) {
            printf("  [%ld] ax=%6d ay=%6d az=%6d gx=%6d gy=%6d gz=%6d\n",
                   sample_count, s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
        }

        activity_t result = classifier_update(&s);
        sample_count++;

        if (result != ACTIVITY_UNKNOWN) {
            window_count++;
            printf("[Window %d] Klasifikasi: %s\n",
                   window_count, activity_name(result));

            if (SHOW_FEATURES) {
                print_features(window_copy, idx);
            }

            idx = 0;
        }

        if (!VERBOSE_RAW && (sample_count % 50 == 0)) {
            printf("  ... %d window, %ld sampel, hasil terakhir: %s\n",
                   window_count, sample_count, activity_name(result));
        }

        simple_delay(SAMPLE_DELAY_LOOPS);
    }

    printf("\nSelesai: %d window diklasifikasi dari %ld sampel.\n",
           window_count, sample_count);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("==============================================\n");
    printf("  Activity Classifier — Hardware Test\n");
    printf("  Platform: PULPissimo + MPU-6050\n");
    printf("==============================================\n\n");

    if (init_sensor() != 0) {
        printf("Test dibatalkan: inisialisasi sensor gagal.\n");
        return 1;
    }

    run_single_window_test();
    run_continuous_test();

    printf("\n==============================================\n");
    printf("  Test hardware selesai.\n");
    printf("==============================================\n");

    return 0;
}

void pe_start(void) {}
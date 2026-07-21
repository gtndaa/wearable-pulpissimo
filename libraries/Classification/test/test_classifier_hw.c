/*
 * Copyright (C) 2026 ICDeC
 *
 * Hardware Test: Activity Classifier on PULPissimo FPGA + MPU-6050
 * ================================================================
 *
 * Test ini menjalankan pipeline classifier LENGKAP di hardware asli:
 *   1. Inisialisasi MPU-6050 (I2C, WHO_AM_I, wake, config)
 *   2. Baca data 6-axis (accel + gyro) melalui adapter
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
 */

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
 * Helper: Nama aktivitas
 * ============================================================================ */

static const char *activity_name(activity_t act)
{
    switch (act) {
        case ACTIVITY_UNKNOWN: return "UNKNOWN";
        case ACTIVITY_STILL:   return "STILL  ";
        case ACTIVITY_SIT:     return "SIT    ";
        case ACTIVITY_STAND:   return "STAND  ";
        case ACTIVITY_WALK:    return "WALK   ";
        case ACTIVITY_RUN:     return "RUN    ";
        case ACTIVITY_FALL:    return "FALL   ";
        default:               return "???    ";
    }
}

/* ============================================================================
 * Test 1: Sensor Init
 *
 * Menggunakan mpu6050 library API (bukan raw I2C).
 * Memastikan sensor terhubung dan terkonfigurasi sebelum classifier jalan.
 * ============================================================================ */

static int test_sensor_init(mpu6050_config_t *cfg)
{
    mpu6050_status_t status;

    printf("[TEST 1] Loading default configuration...\n");
    status = mpu6050_default_config(cfg);
    if (status != MPU6050_OK) {
        printf("  FAIL: mpu6050_default_config() err=%d\n", status);
        return -1;
    }
    printf("  OK: addr=0x%02X, gyro_range=%d, accel_range=%d\n",
           cfg->i2c_addr, cfg->gyro_range, cfg->accel_range);

    printf("[TEST 2] Initializing MPU-6050...\n");
    status = mpu6050_init(cfg);
    if (status != MPU6050_OK) {
        printf("  FAIL: mpu6050_init() err=%d\n", status);
        return -1;
    }
    printf("  OK: Sensor initialized on addr=0x%02X\n\n", cfg->i2c_addr);

    return 0;
}

/* ============================================================================
 * Test 2: Adapter Sanity Check
 *
 * Verifikasi bahwa mpu6050_to_imu_sample() menghasilkan data yang wajar.
 * Saat sensor diam, magnitude accel harus ~1g.
 * ============================================================================ */

static int test_adapter_sanity(void)
{
    printf("[TEST 3] Adapter sanity check (mpu6050_to_imu_sample)...\n");

    imu_sample_t sample;
    mpu6050_status_t status = mpu6050_to_imu_sample(&sample);

    if (status != MPU6050_OK) {
        printf("  FAIL: mpu6050_to_imu_sample() err=%d\n", status);
        return -1;
    }

    printf("  Raw accel: ax=%6d  ay=%6d  az=%6d\n",
           sample.ax, sample.ay, sample.az);
    printf("  Raw gyro:  gx=%6d  gy=%6d  gz=%6d\n",
           sample.gx, sample.gy, sample.gz);

    /* Sanity: magnitude accel raw saat diam harus ~16384 (1g pada ±2g range)
     * Toleransi sangat longgar karena sensor bisa miring:
     * raw² = ax²+ay²+az² ≈ 16384² = 268,435,456
     * Range: 50% - 150% → 134,217,728 - 402,653,184 */
    int32_t ax32 = (int32_t)sample.ax;
    int32_t ay32 = (int32_t)sample.ay;
    int32_t az32 = (int32_t)sample.az;
    /* Hitung dalam satuan (raw/16)² untuk menghindari overflow */
    int32_t mag_scaled = (ax32/16)*(ax32/16) + (ay32/16)*(ay32/16) + (az32/16)*(az32/16);
    /* 1g/16 = 1024, 1024² = 1,048,576. Range: 500,000 - 1,500,000 */
    if (mag_scaled >= 500000 && mag_scaled <= 1500000) {
        printf("  OK: Accel magnitude wajar (~1g)\n\n");
    } else {
        printf("  WARNING: Accel magnitude di luar range (mag_scaled=%d)\n", (int)mag_scaled);
        printf("  (sensor mungkin bergerak, tetap lanjut)\n\n");
    }

    return 0;
}

/* ============================================================================
 * Test 3: Single Window Classification
 *
 * Kumpulkan CLF_WINDOW_SIZE sampel, lalu klasifikasi satu window.
 * Tampilkan fitur yang diekstrak (untuk tuning).
 * ============================================================================ */

static int test_single_window(void)
{
    printf("[TEST 4] Single window classification (%d sampel)...\n",
           CLF_WINDOW_SIZE);

    classifier_init();

    imu_sample_t window_data[CLF_WINDOW_SIZE];
    int i;

    for (i = 0; i < CLF_WINDOW_SIZE; i++) {
        imu_sample_t sample;
        mpu6050_status_t status = mpu6050_to_imu_sample(&sample);
        if (status != MPU6050_OK) {
            printf("  FAIL: Gagal baca sampel ke-%d (err=%d)\n", i, status);
            return -1;
        }
        window_data[i] = sample;

#if VERBOSE_RAW
        printf("  [%2d] ax=%6d ay=%6d az=%6d | gx=%6d gy=%6d gz=%6d\n",
               i, sample.ax, sample.ay, sample.az,
               sample.gx, sample.gy, sample.gz);
#endif

        for (volatile int d = 0; d < SAMPLE_DELAY_LOOPS; d++);
    }

    /* Ekstrak fitur untuk tampilan (tuning) */
    accel_features_t af;
    gyro_features_t gf;
    extract_features_accel(window_data, CLF_WINDOW_SIZE, &af);
    extract_features_gyro(window_data, CLF_WINDOW_SIZE, &gf);

    printf("  --- Fitur Accelerometer ---\n");
    printf("  SMA          = %d (threshold rest: %d)\n",
           (int)af.sma, (int)CLF_SMA_REST_THRESHOLD);
    printf("  SVM_max      = %u (threshold fall: %u)\n",
           (unsigned int)af.svm_max, (unsigned int)CLF_SVM_FALL_THRESHOLD);
    printf("  Tilt ratio   = %d (threshold stand: %d)\n",
           (int)af.tilt_ratio, (int)CLF_TILT_STAND_THRESHOLD);

    printf("  --- Fitur Gyroscope ---\n");
    printf("  Energy       = %d (threshold walk: %d, run: %d, fall: %d)\n",
           (int)gf.energy,
           (int)CLF_GYRO_ENERGY_WALK_THRESHOLD,
           (int)CLF_GYRO_ENERGY_RUN_THRESHOLD,
           (int)CLF_GYRO_ENERGY_FALL_THRESHOLD);
    printf("  ZCR          = %d (threshold walk min: %d)\n",
           (int)gf.zcr, (int)CLF_GYRO_ZCR_WALK_MIN);
    printf("  Mean abs     = %d (threshold still: %d)\n",
           (int)gf.mean_abs, (int)CLF_GYRO_MEAN_ABS_STILL_THRESHOLD);

    /* Klasifikasi menggunakan API langsung (bukan classifier_update,
     * karena kita sudah punya buffer sendiri) */
    classifier_init();
    for (i = 0; i < CLF_WINDOW_SIZE; i++) {
        classifier_push_sample(&window_data[i]);
    }
    activity_t result = classifier_classify();

    printf("  --- Hasil ---\n");
    printf("  Klasifikasi: %s (kode=%d)\n\n", activity_name(result), result);

    return 0;
}

/* ============================================================================
 * Test 4: Continuous Classification
 *
 * Jalankan classifier secara real-time: baca sensor → update → tampilkan
 * hasil setiap kali window penuh.
 *
 * Ini adalah mode operasi sesungguhnya yang akan dipakai di firmware final.
 * ============================================================================ */

static void test_continuous_classification(void)
{
    printf("========================================\n");
    printf(" CONTINUOUS CLASSIFICATION\n");
    printf(" Window size: %d sampel\n", CLF_WINDOW_SIZE);
    if (NUM_WINDOWS > 0)
        printf(" Jumlah window: %d (total %d sampel)\n",
               NUM_WINDOWS, NUM_WINDOWS * CLF_WINDOW_SIZE);
    else
        printf(" Mode: infinite (reset board untuk stop)\n");
    printf("========================================\n\n");

    classifier_init();

    int window_count = 0;
    int sample_count = 0;
    int read_errors = 0;

    /* Shadow buffer: salinan lokal sampel per window.
     * Dibutuhkan karena classifier_update() me-reset window internal
     * setelah classify — tanpa ini kita tidak bisa ekstrak fitur.
     * Biaya: CLF_WINDOW_SIZE × 12 = 384 bytes tambahan di stack.
     * Pada firmware final (bukan debug), buffer ini tidak diperlukan. */
    imu_sample_t shadow_buf[CLF_WINDOW_SIZE];
    int shadow_idx = 0;

    /* Header tabel */
#if SHOW_FEATURES
    printf(" Win |  Result  |    SMA    SVM_max   Tilt |  Energy  ZCR  MeanAbs\n");
    printf("-----+----------+-------------------------+------------------------\n");
#else
    printf(" Win | Sampel |  Result\n");
    printf("-----+--------+---------\n");
#endif

    while (1) {
        imu_sample_t sample;
        mpu6050_status_t status = mpu6050_to_imu_sample(&sample);

        if (status != MPU6050_OK) {
            read_errors++;
            if (read_errors > 10) {
                printf("\n  [!] Terlalu banyak error baca (%d), berhenti.\n",
                       read_errors);
                break;
            }
            for (volatile int d = 0; d < SAMPLE_DELAY_LOOPS; d++);
            continue;
        }

        /* Simpan salinan di shadow buffer */
        shadow_buf[shadow_idx] = sample;
        shadow_idx++;

        sample_count++;
        activity_t result = classifier_update(&sample);

        if (result != ACTIVITY_UNKNOWN) {
            window_count++;

#if SHOW_FEATURES
            /* Ekstrak fitur dari shadow buffer (salinan window yang baru
             * saja diklasifikasi). Ini memungkinkan kita melihat SEMUA
             * nilai fitur + hasil klasifikasi berdampingan — sangat
             * berguna untuk tuning threshold di classifier_config.h. */
            accel_features_t af;
            gyro_features_t gf;
            extract_features_accel(shadow_buf, CLF_WINDOW_SIZE, &af);
            extract_features_gyro(shadow_buf, CLF_WINDOW_SIZE, &gf);

            printf(" %3d | %s | %7d %10u %5d | %7d  %3d  %7d\n",
                   window_count, activity_name(result),
                   (int)af.sma, (unsigned int)af.svm_max, (int)af.tilt_ratio,
                   (int)gf.energy, (int)gf.zcr, (int)gf.mean_abs);
#else
            printf(" %3d | %5d  | %s\n",
                   window_count, sample_count, activity_name(result));
#endif

            /* Reset shadow buffer untuk window berikutnya */
            shadow_idx = 0;

            if (NUM_WINDOWS > 0 && window_count >= NUM_WINDOWS) {
                break;
            }
        }

        for (volatile int d = 0; d < SAMPLE_DELAY_LOOPS; d++);
    }

    printf("\n  Selesai: %d window, %d sampel, %d errors\n",
           window_count, sample_count, read_errors);
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void)
{
    mpu6050_config_t cfg;
    int pass_count = 0;
    int fail_count = 0;

    printf("========================================\n");
    printf(" Activity Classifier — Hardware Test\n");
    printf(" MPU-6050 on ICDeC PULPissimo FPGA\n");
    printf(" Window size: %d sampel\n", CLF_WINDOW_SIZE);
    printf("========================================\n\n");

    /* ---- Test 1 & 2: Sensor Init ---- */
    if (test_sensor_init(&cfg) == 0) {
        printf("  PASS: Sensor init\n\n");
        pass_count++;
    } else {
        printf("  FAIL: Sensor init — berhenti\n");
        fail_count++;
        goto done;
    }

    /* ---- Test 3: Adapter Sanity ---- */
    if (test_adapter_sanity() == 0) {
        printf("  PASS: Adapter sanity\n\n");
        pass_count++;
    } else {
        printf("  FAIL: Adapter sanity\n\n");
        fail_count++;
    }

    /* ---- Test 4: Single Window Classification ---- */
    if (test_single_window() == 0) {
        printf("  PASS: Single window classification\n\n");
        pass_count++;
    } else {
        printf("  FAIL: Single window classification\n\n");
        fail_count++;
    }

    /* ---- Results ---- */
    printf("========================================\n");
    printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
    printf("========================================\n");

    /* ---- Continuous Classification (hanya jika semua test pass) ---- */
    if (fail_count == 0) {
        test_continuous_classification();
    }

done:
    mpu6050_deinit();

    printf("\n========================================\n");
    printf(" TEST SELESAI\n");
    printf("========================================\n");

    return (fail_count == 0) ? 0 : -1;
}

/* PULPissimo runtime membutuhkan stub ini */
void pe_start(void)
{
}

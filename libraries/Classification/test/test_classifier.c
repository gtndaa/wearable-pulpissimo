/*
 * Copyright (C) 2026 ICDeC
 *
 * Unit Test — Activity Classifier
 *
 * Test ini MEMBUKTIKAN bahwa modul classifier:
 *   1. Bisa dikompilasi TANPA hardware (host GCC biasa)
 *   2. Tidak bergantung pada driver sensor spesifik mana pun
 *   3. Menghasilkan output yang konsisten untuk data dummy yang diketahui
 *
 * Test menggunakan array imu_sample_t buatan (mock) untuk setiap skenario:
 *   - Still (diam): hanya gravitasi statis, gyro mendekati nol
 *   - Walk (jalan): akselerasi periodik sedang, gyro periodik
 *   - Run (lari): akselerasi periodik tinggi, gyro energy tinggi
 *   - Fall (jatuh): benturan sangat tinggi + rotasi cepat sesaat
 *
 * CATATAN: Nilai threshold di classifier_config.h adalah PLACEHOLDER.
 * Test ini menggunakan data dummy yang disesuaikan agar melewati
 * threshold placeholder tersebut. Setelah tuning dengan data nyata,
 * test ini perlu diperbarui agar konsisten dengan threshold baru.
 *
 * Kompilasi: gcc -Wall -Wextra -I.. -o test_classifier test_classifier.c ../classifier.c
 */

#include <stdio.h>
#include <string.h>

/* PENTING: hanya include header classifier, BUKAN header sensor apa pun.
 * Ini membuktikan decoupling classifier dari driver sensor. */
#include "classifier.h"
#include "classifier_config.h"

/* ============================================================================
 * Test Framework Sederhana (tanpa dependensi eksternal)
 * ============================================================================ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  [FAIL] %s\n", msg); \
    } \
} while (0)

/* ============================================================================
 * Helper: nama aktivitas untuk output readable
 * ============================================================================ */

static const char *activity_name(activity_t act)
{
    switch (act) {
        case ACTIVITY_UNKNOWN: return "UNKNOWN";
        case ACTIVITY_STILL:   return "STILL";
        case ACTIVITY_SIT:     return "SIT";
        case ACTIVITY_STAND:   return "STAND";
        case ACTIVITY_WALK:    return "WALK";
        case ACTIVITY_RUN:     return "RUN";
        case ACTIVITY_FALL:    return "FALL";
        default:               return "???";
    }
}

/* ============================================================================
 * Helper: isi window dengan data yang sama (untuk simulasi steady-state)
 * ============================================================================ */

/**
 * Helper: feed beberapa window identik agar majority vote stabil.
 * Perlu minimal CLF_HISTORY_SIZE window agar vote menghasilkan
 * hasil yang konsisten (bukan terpengaruh riwayat sebelumnya).
 */
static activity_t feed_constant_windows(int16_t ax, int16_t ay, int16_t az,
                                        int16_t gx, int16_t gy, int16_t gz,
                                        int num_windows)
{
    activity_t result = ACTIVITY_UNKNOWN;

    classifier_init();
    for (int w = 0; w < num_windows; w++) {
        for (int i = 0; i < CLF_WINDOW_SIZE; i++) {
            imu_sample_t s;
            s.ax = ax; s.ay = ay; s.az = az;
            s.gx = gx; s.gy = gy; s.gz = gz;
            result = classifier_update(&s);
        }
    }
    return result;
}

/* ============================================================================
 * Test 1: classifier_init() me-reset state
 * ============================================================================ */

static void test_init_resets_state(void)
{
    printf("\n--- Test: classifier_init() resets state ---\n");

    /* Push beberapa sampel, lalu init ulang, sampel baru harus mulai
     * dari awal (window tidak penuh setelah < CLF_WINDOW_SIZE push) */
    classifier_init();

    imu_sample_t s = {0, 0, 16384, 0, 0, 0}; /* diam, 1g di sumbu Z */
    for (int i = 0; i < CLF_WINDOW_SIZE / 2; i++) {
        classifier_push_sample(&s);
    }

    /* Reset */
    classifier_init();

    /* Harus butuh CLF_WINDOW_SIZE sampel penuh lagi */
    int full = 0;
    for (int i = 0; i < CLF_WINDOW_SIZE - 1; i++) {
        full = classifier_push_sample(&s);
    }
    TEST_ASSERT(full == 0, "Window belum penuh setelah N-1 sampel post-reset");

    full = classifier_push_sample(&s);
    TEST_ASSERT(full == 1, "Window penuh setelah N sampel post-reset");
}

/* ============================================================================
 * Test 2: Data diam → STILL
 *
 * Simulasi: sensor diam sempurna
 *   Accelerometer: hanya gravitasi statis (0, 0, ~1g)
 *   Gyroscope: nol (tidak ada rotasi)
 *
 * Untuk MPU-6050 ±2g (16384 LSB/g):
 *   ax=0, ay=0, az=16384 (1g di sumbu Z)
 *   gx=0, gy=0, gz=0
 *
 * Expected: SMA rendah (hanya gravitasi), gyro mean_abs sangat rendah → STILL
 * ============================================================================ */

static void test_still_detection(void)
{
    printf("\n--- Test: Data diam → STILL ---\n");

    activity_t result = feed_constant_windows(0, 0, 16384, 0, 0, 0,
                                               CLF_HISTORY_SIZE);

    printf("  Hasil klasifikasi: %s (%d)\n", activity_name(result), result);
    TEST_ASSERT(result == ACTIVITY_STILL,
                "Sensor diam sempurna → STILL");
}

/* ============================================================================
 * Test 3: Data diam + postur berdiri → STAND
 *
 * Simulasi: sensor diam tapi dengan sedikit noise gyro (bukan nol sempurna),
 *   meniru kondisi berdiri dengan micro-sway tubuh
 *
 * Expected: SMA rendah, gyro mean_abs di atas STILL threshold,
 *           tilt_ratio tinggi (az dominan) → STAND
 * ============================================================================ */

static void test_stand_detection(void)
{
    printf("\n--- Test: Data diam + postur tegak → STAND ---\n");

    classifier_init();
    activity_t result = ACTIVITY_UNKNOWN;

    /* Feed CLF_HISTORY_SIZE windows agar majority vote stabil */
    for (int w = 0; w < CLF_HISTORY_SIZE; w++) {
        for (int i = 0; i < CLF_WINDOW_SIZE; i++) {
            imu_sample_t s;
            s.ax = 200;            /* sedikit noise accel */
            s.ay = -150;
            s.az = 16300;          /* dominan gravitasi di Z → tegak */
            /* Gyro mean_abs harus > CLF_GYRO_MEAN_ABS_STILL_THRESHOLD (50000)
             * per window. mean_abs = N × (|gx|+|gy|+|gz|)
             * 32 × (800+600+400) = 57600 > 50000 ✓ */
            s.gx = (i % 2 == 0) ? 800 : -800;
            s.gy = 600;
            s.gz = -400;
            result = classifier_update(&s);
        }
    }

    printf("  Hasil klasifikasi: %s (%d)\n", activity_name(result), result);
    TEST_ASSERT(result == ACTIVITY_STAND,
                "Diam + sumbu Z dominan + gyro sedikit → STAND");
}

/* ============================================================================
 * Test 4: Data diam + postur duduk → SIT
 *
 * Simulasi: sensor diam tapi sensor miring (duduk — sumbu Z tidak dominan)
 *   Orientasi: Z kecil, X atau Y lebih besar → sensor condong
 *
 * Expected: SMA rendah, tilt_ratio rendah (az tidak dominan) → SIT
 * ============================================================================ */

static void test_sit_detection(void)
{
    printf("\n--- Test: Data diam + postur condong → SIT ---\n");

    classifier_init();
    activity_t result = ACTIVITY_UNKNOWN;

    /* Feed CLF_HISTORY_SIZE windows agar majority vote stabil */
    for (int w = 0; w < CLF_HISTORY_SIZE; w++) {
        for (int i = 0; i < CLF_WINDOW_SIZE; i++) {
            imu_sample_t s;
            /* SMA per sampel harus < CLF_SMA_REST_THRESHOLD / N agar masuk rest.
             * CLF_SMA_REST_THRESHOLD=612000, N=32 → max ~19125 per sampel.
             * Tapi Z harus TIDAK dominan agar tilt_ratio rendah → SIT.
             * Total ~15900 per sampel. */
            s.ax = 8000;           /* sensor miring — X besar relatif */
            s.ay = 4000;           /* Y signifikan */
            s.az = 3000;           /* Z kecil → bukan tegak → tilt rendah */
            /* Gyro mean_abs > 50000: 32 × (800+600+400) = 57600 ✓ */
            s.gx = (i % 2 == 0) ? 800 : -800;
            s.gy = 600;
            s.gz = -400;
            result = classifier_update(&s);
        }
    }

    printf("  Hasil klasifikasi: %s (%d)\n", activity_name(result), result);
    TEST_ASSERT(result == ACTIVITY_SIT,
                "Diam + sensor condong (az kecil) → SIT");
}

/* ============================================================================
 * Test 5: Data jalan → WALK
 *
 * Simulasi: pola akselerasi periodik sedang + gyro periodik
 *   - Akselerasi naik-turun meniru langkah kaki
 *   - Gyro berosilasi meniru ayunan tangan
 *   - SMA harus di atas rest threshold
 *   - ZCR gyro harus di atas minimum walk
 *   - Energy gyro di antara walk dan run threshold
 * ============================================================================ */

static void test_walk_detection(void)
{
    printf("\n--- Test: Data pola jalan → WALK ---\n");

    classifier_init();
    activity_t result = ACTIVITY_UNKNOWN;

    /* Feed CLF_HISTORY_SIZE windows agar majority vote stabil */
    for (int w = 0; w < CLF_HISTORY_SIZE; w++) {
        for (int i = 0; i < CLF_WINDOW_SIZE; i++) {
            imu_sample_t s;
            /* Pola akselerasi periodik (meniru langkah): naik-turun */
            int16_t step_phase = (i % 8 < 4) ? 5000 : -3000;
            s.ax = 2000 + step_phase;
            s.ay = 1000;
            s.az = 16000 + step_phase / 2;

            /* Pola gyro periodik (meniru ayunan tangan)
             * Energy harus antara WALK (150K) dan RUN (5M).
             * Per sampel: (600>>4)² + (400>>4)² + (300>>4)²
             *           = 37² + 25² + 18² = 1369+625+324 = 2318
             * × 32 sampel = 74,176 ... terlalu rendah.
             * Pakai gx=±1200, gy=±800, gz=±600:
             * (1200>>4)² + (800>>4)² + (600>>4)²
             * = 75² + 50² + 37² = 5625+2500+1369 = 9494
             * × 32 = 303,808 → antara 150K dan 5M ✓ */
            s.gx = (i % 4 < 2) ? 1200 : -1200;
            s.gy = (i % 4 < 2) ? 800 : -800;
            s.gz = (i % 4 < 2) ? 600 : -600;

            result = classifier_update(&s);
        }
    }

    printf("  Hasil klasifikasi: %s (%d)\n", activity_name(result), result);
    TEST_ASSERT(result == ACTIVITY_WALK,
                "Pola akselerasi + gyro periodik sedang → WALK");
}

/* ============================================================================
 * Test 6: Data lari → RUN
 *
 * Simulasi: sama seperti walk tapi dengan intensitas lebih tinggi
 *   - Akselerasi jauh lebih besar
 *   - Gyro energy jauh lebih tinggi
 *   - ZCR tetap tinggi (masih periodik)
 * ============================================================================ */

static void test_run_detection(void)
{
    printf("\n--- Test: Data pola lari → RUN ---\n");

    classifier_init();
    activity_t result = ACTIVITY_UNKNOWN;

    /* Feed CLF_HISTORY_SIZE windows agar majority vote stabil */
    for (int w = 0; w < CLF_HISTORY_SIZE; w++) {
        for (int i = 0; i < CLF_WINDOW_SIZE; i++) {
            imu_sample_t s;
            /* Akselerasi tinggi dan periodik */
            int16_t step_phase = (i % 6 < 3) ? 10000 : -8000;
            s.ax = 4000 + step_phase;
            s.ay = 3000;
            s.az = 16000 + step_phase / 2;

            /* Gyro energy harus > RUN threshold (5M).
             * Per sampel: (8000>>4)² + (6000>>4)² + (4500>>4)²
             *           = 500² + 375² + 281² = 250000+140625+78961 = 469586
             * × 32 = 15,026,752 → > 5M ✓ */
            s.gx = (i % 3 < 2) ? 8000 : -8000;
            s.gy = (i % 3 < 2) ? 6000 : -6000;
            s.gz = (i % 3 < 2) ? 4500 : -4500;

            result = classifier_update(&s);
        }
    }

    printf("  Hasil klasifikasi: %s (%d)\n", activity_name(result), result);
    TEST_ASSERT(result == ACTIVITY_RUN,
                "Pola akselerasi + gyro periodik tinggi → RUN");
}

/* ============================================================================
 * Test 7: Data jatuh → FALL
 *
 * Simulasi: benturan keras + rotasi cepat
 *   - SVM peak sangat tinggi (impact besar)
 *   - Gyro energy sangat tinggi (rotasi tidak terkontrol)
 *   - Kedua kondisi harus terpenuhi bersamaan
 * ============================================================================ */

static void test_fall_detection(void)
{
    printf("\n--- Test: Data benturan + rotasi cepat → FALL ---\n");

    classifier_init();
    activity_t result = ACTIVITY_UNKNOWN;

    for (int i = 0; i < CLF_WINDOW_SIZE; i++) {
        imu_sample_t s;

        if (i < CLF_WINDOW_SIZE / 2) {
            /* Fase awal: akselerasi tinggi tapi belum impact */
            s.ax = 5000;
            s.ay = 5000;
            s.az = 20000;
            /* Gyro tinggi di seluruh window agar total energy > 5M (fall threshold)
             * Per sampel: (10000>>4)² + (8000>>4)² + (7000>>4)² = 625²+500²+437²
             *           = 390625+250000+190969 = 831594
             * × 32 = 26,611,008 → > 5M ✓ */
            s.gx = 10000;
            s.gy = 8000;
            s.gz = 7000;
        } else {
            /* Fase impact: akselerasi SANGAT tinggi + rotasi cepat */
            s.ax = 25000;  /* >1.5g pada ±2g range → benturan keras */
            s.ay = 20000;
            s.az = 30000;
            s.gx = 12000; /* rotasi sangat cepat */
            s.gy = 10000;
            s.gz = 9000;
        }

        result = classifier_update(&s);
    }

    printf("  Hasil klasifikasi: %s (%d)\n", activity_name(result), result);
    TEST_ASSERT(result == ACTIVITY_FALL,
                "Benturan tinggi + rotasi cepat → FALL");
}

/* ============================================================================
 * Test 8: Feature extraction langsung (white-box test)
 *
 * Memastikan fungsi extract_features_accel() dan extract_features_gyro()
 * menghasilkan nilai yang masuk akal untuk input yang diketahui.
 * ============================================================================ */

static void test_feature_extraction(void)
{
    printf("\n--- Test: Feature extraction values ---\n");

    /* Buat window dengan nilai konstan yang mudah dihitung secara manual */
    imu_sample_t buf[4];
    for (int i = 0; i < 4; i++) {
        buf[i].ax = 100;
        buf[i].ay = 200;
        buf[i].az = 300;
        buf[i].gx = (i % 2 == 0) ? 500 : -500;  /* berosilasi untuk ZCR */
        buf[i].gy = 100;
        buf[i].gz = 100;
    }

    accel_features_t af;
    gyro_features_t gf;

    extract_features_accel(buf, 4, &af);
    extract_features_gyro(buf, 4, &gf);

    /* SMA = 4 × (100+200+300) = 2400 */
    printf("  SMA = %ld (expected 2400)\n", (long)af.sma);
    TEST_ASSERT(af.sma == 2400, "SMA = Σ(|ax|+|ay|+|az|) = 2400");

    /* SVM per sampel = 100² + 200² + 300² = 10000 + 40000 + 90000 = 140000
     * Semua sampel sama → max = 140000 */
    printf("  SVM_max = %lu (expected 140000)\n", (unsigned long)af.svm_max);
    TEST_ASSERT(af.svm_max == 140000UL, "SVM_max = 140000 (kuadrat, tanpa sqrt)");

    /* ZCR: gx berosilasi +500, -500, +500, -500 → 3 zero-crossings
     * (antara sampel 0-1, 1-2, 2-3) */
    printf("  ZCR = %ld (expected 3)\n", (long)gf.zcr);
    TEST_ASSERT(gf.zcr == 3, "ZCR = 3 (tiga perubahan tanda gx)");

    /* Energy: gx>>4 = ±31, gy>>4 = 6, gz>>4 = 6
     * Per sampel: 31² + 6² + 6² = 961 + 36 + 36 = 1033
     * × 4 = 4132 */
    printf("  Gyro energy = %ld (expected ~4132)\n", (long)gf.energy);
    TEST_ASSERT(gf.energy > 0, "Gyro energy > 0 (ada rotasi)");
}

/* ============================================================================
 * Test 9: classifier_update() returns UNKNOWN sampai window penuh
 * ============================================================================ */

static void test_update_returns_unknown_until_full(void)
{
    printf("\n--- Test: classifier_update() → UNKNOWN sampai window penuh ---\n");

    classifier_init();
    imu_sample_t s = {0, 0, 16384, 0, 0, 0};

    int unknown_count = 0;
    for (int i = 0; i < CLF_WINDOW_SIZE - 1; i++) {
        activity_t r = classifier_update(&s);
        if (r == ACTIVITY_UNKNOWN) {
            unknown_count++;
        }
    }

    TEST_ASSERT(unknown_count == CLF_WINDOW_SIZE - 1,
                "Semua N-1 update pertama → UNKNOWN");

    /* Sampel ke-N harus menghasilkan klasifikasi nyata */
    activity_t final = classifier_update(&s);
    TEST_ASSERT(final != ACTIVITY_UNKNOWN,
                "Update ke-N → hasil klasifikasi (bukan UNKNOWN)");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("==============================================\n");
    printf("  Activity Classifier — Unit Tests\n");
    printf("  Window size: %d sampel\n", CLF_WINDOW_SIZE);
    printf("==============================================\n");

    test_init_resets_state();
    test_still_detection();
    test_stand_detection();
    test_sit_detection();
    test_walk_detection();
    test_run_detection();
    test_fall_detection();
    test_feature_extraction();
    test_update_returns_unknown_until_full();

    printf("\n==============================================\n");
    printf("  HASIL: %d PASSED, %d FAILED (dari %d test)\n",
           tests_passed, tests_failed, tests_run);
    printf("==============================================\n");

    return tests_failed > 0 ? 1 : 0;
}

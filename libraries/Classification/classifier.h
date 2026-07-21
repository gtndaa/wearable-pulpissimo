/*
 * Copyright (C) 2026 ICDeC
 *
 * Activity Classifier — Public API
 *
 * Modul klasifikasi aktivitas real-time berbasis fusi data gyroscope +
 * accelerometer. Menggunakan hierarchical decision tree (terinspirasi
 * Karantonis et al. 2006) dengan fitur dari kedua sensor.
 *
 * PRINSIP UTAMA:
 *   1. SENSOR-AGNOSTIC — hanya menerima imu_sample_t, tidak pernah
 *      include header driver sensor spesifik apa pun
 *   2. INTEGER-ONLY — tidak ada float/double, semua fixed-point
 *   3. HEMAT MEMORI — total RAM < 500 bytes (window + fitur)
 *   4. ZERO DEPENDENCIES — bisa dikompilasi tanpa hardware / PULP SDK
 *
 * PIPELINE:
 *   imu_sample_t → sliding window → feature extraction → decision tree
 *                                    (accel + gyro)       (hierarchical)
 *
 * USAGE:
 *   classifier_init();
 *   while (1) {
 *       imu_sample_t sample;
 *       // ... isi sample dari adapter sensor ...
 *       activity_t act = classifier_update(&sample);
 *       if (act != ACTIVITY_UNKNOWN) {
 *           // window penuh, hasil klasifikasi tersedia
 *       }
 *   }
 */

#ifndef __CLASSIFIER_H__
#define __CLASSIFIER_H__

#include "imu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Feature Structures
 *
 * Dipisah menjadi accel dan gyro agar jelas asal-usul setiap fitur.
 * Masing-masing sensor berkontribusi fitur yang berbeda:
 *   - Accelerometer → postur/orientasi (tilt) + intensitas benturan (SVM)
 *   - Gyroscope → pola gerakan rotasional (energy, ZCR)
 * ============================================================================ */

/**
 * Fitur yang diekstrak dari komponen ACCELEROMETER (ax, ay, az).
 *
 * Peran:
 *   - SMA: membedakan rest vs active (level 1 decision tree)
 *   - tilt_ratio: membedakan postur sit vs stand (level 2)
 *   - SVM peak: deteksi benturan/fall (level 4, bersama gyro)
 */
typedef struct {
    int32_t  sma;          /**< Signal Magnitude Area: Σ(|ax|+|ay|+|az|)
                                TIDAK dibagi N — threshold dikalikan N
                                (trik Karantonis untuk menghindari pembagian) */

    uint32_t svm_max;      /**< Signal Vector Magnitude peak (kuadrat):
                                max(ax² + ay² + az²) dalam window
                                TANPA sqrt — threshold juga dikuadratkan
                                uint32_t karena 32767² × 3 bisa > int32_t */

    int32_t  tilt_ratio;   /**< Proxy tilt angle (fixed-point ×1024):
                                (mean_az × 1024) / (|mean_ax|+|mean_ay|+|mean_az|+1)
                                Mendekati 1024 → vertikal (berdiri)
                                Mendekati 0 → horizontal (duduk/rebah)
                                Menggantikan arccos yang terlalu mahal */
} accel_features_t;

/**
 * Fitur yang diekstrak dari komponen GYROSCOPE (gx, gy, gz).
 *
 * Peran:
 *   - energy: membedakan intensitas gerakan rotasional (walk vs run)
 *   - zcr: mendeteksi periodisitas (pola langkah)
 *   - mean_abs: secondary check untuk konfirmasi rest/still
 */
typedef struct {
    int32_t energy;        /**< Gyro energy: Σ((gx>>2)² + (gy>>2)² + (gz>>2)²)
                                Right-shift 2 SEBELUM kuadrat untuk menghindari
                                overflow int32_t (max 32767² × 3 > INT32_MAX)
                                TIDAK dibagi N */

    int32_t zcr;           /**< Zero-Crossing Rate: jumlah perubahan tanda
                                pada gx antar sampel berurutan dalam window.
                                Gerakan periodik (jalan/lari) → ZCR tinggi.
                                Diam/drift konstan → ZCR rendah */

    int32_t mean_abs;      /**< Mean absolute: Σ(|gx| + |gy| + |gz|)
                                TIDAK dibagi N (trik Karantonis).
                                Dipakai sebagai secondary check rest vs active */
} gyro_features_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * Inisialisasi state internal classifier.
 * WAJIB dipanggil sekali sebelum classifier_update() atau classifier_push_sample().
 * Mereset sliding window buffer dan semua counter.
 */
void classifier_init(void);

/**
 * Push satu sampel IMU ke sliding window buffer.
 *
 * @param sample  Pointer ke sampel IMU (TIDAK boleh NULL)
 * @return 1 jika window sudah penuh (siap untuk klasifikasi),
 *         0 jika window belum penuh
 */
int classifier_push_sample(const imu_sample_t *sample);

/**
 * Jalankan feature extraction + klasifikasi pada window yang sudah penuh.
 * Memanggil extract_features_accel(), extract_features_gyro(), lalu
 * hierarchical decision tree.
 *
 * HANYA panggil setelah classifier_push_sample() return 1.
 * Setelah dipanggil, window di-reset untuk mulai mengumpulkan sampel baru.
 *
 * @return Kelas aktivitas yang terdeteksi (activity_t)
 */
activity_t classifier_classify(void);

/**
 * Convenience function: push sampel + auto-classify saat window penuh.
 *
 * Menggabungkan classifier_push_sample() dan classifier_classify().
 * Return ACTIVITY_UNKNOWN selama window belum penuh.
 *
 * Typical usage dalam loop utama:
 *   activity_t result = classifier_update(&sample);
 *   if (result != ACTIVITY_UNKNOWN) { ... }
 *
 * @param sample  Pointer ke sampel IMU (TIDAK boleh NULL)
 * @return Kelas aktivitas jika window penuh, ACTIVITY_UNKNOWN jika belum
 */
activity_t classifier_update(const imu_sample_t *sample);

/**
 * Ekstrak fitur dari komponen accelerometer dalam window.
 * Diekspos untuk keperluan debugging dan tuning threshold offline.
 *
 * @param buf    Array of imu_sample_t (minimal n elemen)
 * @param n      Jumlah sampel dalam buffer
 * @param feat   Output struct fitur accelerometer
 */
void extract_features_accel(const imu_sample_t *buf, int n,
                            accel_features_t *feat);

/**
 * Ekstrak fitur dari komponen gyroscope dalam window.
 * Diekspos untuk keperluan debugging dan tuning threshold offline.
 *
 * @param buf    Array of imu_sample_t (minimal n elemen)
 * @param n      Jumlah sampel dalam buffer
 * @param feat   Output struct fitur gyroscope
 */
void extract_features_gyro(const imu_sample_t *buf, int n,
                           gyro_features_t *feat);

#ifdef __cplusplus
}
#endif

#endif /* __CLASSIFIER_H__ */

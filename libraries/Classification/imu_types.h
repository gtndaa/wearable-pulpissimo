/*
 * Copyright (C) 2026 ICDeC
 *
 * Sensor Abstraction Layer — Generic IMU Data Types
 *
 * File ini mendefinisikan tipe data generik untuk data IMU (Inertial
 * Measurement Unit) yang TIDAK terikat pada sensor spesifik mana pun.
 *
 * PRINSIP DESAIN:
 *   Modul klasifikasi (classifier.c/h) HANYA bergantung pada tipe-tipe
 *   yang didefinisikan di sini. Ia tidak pernah meng-include header driver
 *   sensor tertentu (mis. mpu6050.h, l3g4200d.h). Dengan demikian:
 *
 *   1. Classifier bisa dikompilasi dan diuji TANPA hardware apa pun
 *      (cukup dengan array imu_sample_t buatan/mock).
 *   2. Saat mengganti sensor (mis. dari MPU-6050 ke L3G4200D + accel
 *      terpisah), cukup buat fungsi adapter baru yang mengisi
 *      imu_sample_t — classifier TIDAK perlu diubah.
 *
 * CATATAN TIPE DATA:
 *   Semua field di imu_sample_t menggunakan int16_t RAW dari register
 *   sensor, BUKAN nilai terkonversi (milli-g, dps, dll). Alasan:
 *   - Menghindari konversi ganda (sensor → unit fisik → fitur)
 *   - Threshold di classifier akan di-tuning langsung terhadap skala
 *     raw yang dipakai (mis. ±2g = 16384 LSB/g)
 *   - Konsisten dengan pendekatan Karantonis et al. (2006) yang bekerja
 *     langsung dari ADC output
 */

#ifndef __IMU_TYPES_H__
#define __IMU_TYPES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Generic IMU Sample
 *
 * Satu sampel gabungan accelerometer + gyroscope.
 * Ukuran: 6 × int16_t = 12 bytes per sampel.
 *
 * Driver sensor mana pun (MPU-6050, L3G4200D + accel terpisah, dll)
 * bertanggung jawab mengisi struct ini lewat fungsi adapter masing-masing
 * (mis. mpu6050_to_imu_sample()), yang BERADA DI LUAR modul klasifikasi.
 * ============================================================================ */

typedef struct {
    int16_t ax;   /**< Accelerometer sumbu X (raw, LSB) */
    int16_t ay;   /**< Accelerometer sumbu Y (raw, LSB) */
    int16_t az;   /**< Accelerometer sumbu Z (raw, LSB) */
    int16_t gx;   /**< Gyroscope sumbu X (raw, LSB) */
    int16_t gy;   /**< Gyroscope sumbu Y (raw, LSB) */
    int16_t gz;   /**< Gyroscope sumbu Z (raw, LSB) */
} imu_sample_t;

/* ============================================================================
 * Activity Classes
 *
 * Enum kelas aktivitas yang dikenali classifier.
 * Urutan sengaja dimulai dari 0 (UNKNOWN) agar bisa dipakai sebagai
 * indeks array jika diperlukan (mis. untuk logging/statistik).
 * ============================================================================ */

typedef enum {
    ACTIVITY_UNKNOWN = 0,  /**< Belum diklasifikasi / tidak dikenali */
    ACTIVITY_STILL,        /**< Diam total (perangkat tidak bergerak) */
    ACTIVITY_SIT,          /**< Duduk (postur condong, sedikit gerakan) */
    ACTIVITY_STAND,        /**< Berdiri (postur tegak, sedikit gerakan) */
    ACTIVITY_WALK,         /**< Berjalan (pola langkah periodik) */
    ACTIVITY_RUN,          /**< Berlari (pola langkah cepat, energi tinggi) */
    ACTIVITY_FALL,         /**< Jatuh (benturan tinggi + rotasi cepat) */
    ACTIVITY_COUNT         /**< Jumlah kelas (untuk iterasi/array sizing) */
} activity_t;

#ifdef __cplusplus
}
#endif

#endif /* __IMU_TYPES_H__ */

/*
 * Copyright (C) 2026 ICDeC
 *
 * Sensor Abstraction Layer — Generic IMU Data Types
 *
 * Tipe data generik untuk data IMU yang TIDAK terikat sensor spesifik.
 * Classifier hanya bergantung pada file ini — tidak pernah include
 * header driver sensor. Lihat README.md untuk detail arsitektur.
 */

#ifndef __IMU_TYPES_H__
#define __IMU_TYPES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Satu sampel IMU 6-axis (accel + gyro). Semua field raw (int16_t).
 *  Ukuran: 12 bytes. */
typedef struct {
    int16_t ax;   /**< Accelerometer X (raw LSB) */
    int16_t ay;   /**< Accelerometer Y (raw LSB) */
    int16_t az;   /**< Accelerometer Z (raw LSB) */
    int16_t gx;   /**< Gyroscope X (raw LSB) */
    int16_t gy;   /**< Gyroscope Y (raw LSB) */
    int16_t gz;   /**< Gyroscope Z (raw LSB) */
} imu_sample_t;

/** Kelas aktivitas yang dikenali classifier. */
typedef enum {
    ACTIVITY_UNKNOWN = 0,  /**< Belum diklasifikasi */
    ACTIVITY_STILL,        /**< Diam total */
    ACTIVITY_SIT,          /**< Duduk */
    ACTIVITY_STAND,        /**< Berdiri */
    ACTIVITY_LIE,          /**< Tiduran / rebahan (badan horizontal) */
    ACTIVITY_WALK,         /**< Berjalan */
    ACTIVITY_RUN,          /**< Berlari */
    ACTIVITY_FALL,         /**< Jatuh */
    ACTIVITY_COUNT         /**< Jumlah kelas (untuk array sizing) */
} activity_t;

#ifdef __cplusplus
}
#endif

#endif /* __IMU_TYPES_H__ */
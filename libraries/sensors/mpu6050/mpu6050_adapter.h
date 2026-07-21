/*
 * Copyright (C) 2026 ICDeC
 *
 * MPU-6050 Adapter — Contoh Integrasi Sensor ke Classifier
 *
 * File ini BUKAN bagian dari modul klasifikasi. Ia berada di folder
 * sensors/mpu6050/ dan berfungsi sebagai CONTOH cara mengisi struct
 * generik imu_sample_t dari driver MPU-6050 yang sudah ada.
 *
 * PRINSIP:
 *   - Adapter ini meng-include KEDUA header: mpu6050.h (driver sensor)
 *     DAN imu_types.h (kontrak data classifier).
 *   - Modul classifier (classifier.c/h) TIDAK PERNAH meng-include file ini
 *     atau mpu6050.h — ia hanya tahu imu_types.h.
 *   - Saat sensor diganti (mis. L3G4200D + accelerometer terpisah),
 *     buat adapter baru di folder sensor baru, classifier TIDAK perlu diubah.
 *
 * USAGE dalam main loop:
 *
 *   #include "mpu6050.h"
 *   #include "mpu6050_adapter.h"
 *   #include "../../Classification/classifier.h"
 *
 *   classifier_init();
 *   while (1) {
 *       imu_sample_t sample;
 *       if (mpu6050_to_imu_sample(&sample) == MPU6050_OK) {
 *           activity_t act = classifier_update(&sample);
 *           if (act != ACTIVITY_UNKNOWN) {
 *               printf("Aktivitas: %d\n", act);
 *           }
 *       }
 *       // ... delay/timer untuk sampling rate ...
 *   }
 */

#ifndef __MPU6050_ADAPTER_H__
#define __MPU6050_ADAPTER_H__

#include "mpu6050.h"
#include "../../Classification/imu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Baca data raw dari MPU-6050 (accel + gyro) dan isi ke struct generik
 * imu_sample_t.
 *
 * Fungsi ini melakukan DUA pembacaan I2C:
 *   1. mpu6050_accel_read_raw() → ax, ay, az
 *   2. mpu6050_gyro_read_raw()  → gx, gy, gz
 *
 * Kedua pembacaan menghasilkan int16_t raw (langsung dari register sensor),
 * yang langsung di-copy ke field imu_sample_t yang juga int16_t raw.
 * Tidak ada konversi unit di sini — classifier bekerja dengan raw values.
 *
 * CATATAN: Idealnya, untuk mengurangi overhead I2C, kedua pembacaan bisa
 * digabung menjadi satu burst-read dari register 0x3B (ACCEL_XOUT_H)
 * sampai 0x48 (GYRO_ZOUT_L) = 14 bytes (6 accel + 2 temp + 6 gyro).
 * Ini adalah optimisasi opsional yang bisa dilakukan nanti.
 *
 * @param out  Pointer ke imu_sample_t yang akan diisi (TIDAK boleh NULL)
 * @return MPU6050_OK jika berhasil, error code jika gagal
 */
static inline mpu6050_status_t mpu6050_to_imu_sample(imu_sample_t *out)
{
    mpu6050_accel_raw_t accel;
    mpu6050_raw_t gyro;
    mpu6050_status_t status;

    /* Baca accelerometer raw (3-axis, int16_t per sumbu) */
    status = mpu6050_accel_read_raw(&accel);
    if (status != MPU6050_OK) {
        return status;
    }

    /* Baca gyroscope raw (3-axis, int16_t per sumbu) */
    status = mpu6050_gyro_read_raw(&gyro);
    if (status != MPU6050_OK) {
        return status;
    }

    /* Mapping langsung: tipe data sama (int16_t), tidak perlu konversi */
    out->ax = accel.x;
    out->ay = accel.y;
    out->az = accel.z;
    out->gx = gyro.x;
    out->gy = gyro.y;
    out->gz = gyro.z;

    return MPU6050_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_ADAPTER_H__ */

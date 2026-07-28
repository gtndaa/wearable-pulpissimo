/*
 * Copyright (C) 2026 ICDeC
 *
 * Activity Classifier — Public API
 *
 * Klasifikasi aktivitas real-time: sensor-agnostic, integer-only,
 * hemat memori (< 500 bytes RAM).
 *
 * Pipeline: imu_sample_t → window → features → decision tree → vote
 *
 * Lihat README.md untuk penjelasan lengkap.
 */

#ifndef __CLASSIFIER_H__
#define __CLASSIFIER_H__

#include "imu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Feature Structures ---- */

/** Fitur accelerometer: SMA, SVM peak, tilt ratio. */
typedef struct {
    int32_t  sma;          /**< Σ(|ax|+|ay|+|az|), tidak dibagi N */
    uint32_t svm_max;      /**< max(ax²+ay²+az²), tanpa sqrt, uint32_t */
    int32_t  tilt_ratio;   /**< (sum_az×1024)/(|sum_ax|+|sum_ay|+|sum_az|+N) */
} accel_features_t;

/** Fitur gyroscope: energy, ZCR, mean absolute. */
typedef struct {
    int32_t energy;        /**< Σ((gx>>4)²+(gy>>4)²+(gz>>4)²), tidak dibagi N */
    int32_t zcr;           /**< Zero-crossing count pada gx */
    int32_t mean_abs;      /**< Σ(|gx|+|gy|+|gz|), tidak dibagi N */
} gyro_features_t;

/* ---- Public API ---- */

/** Inisialisasi classifier. Wajib dipanggil sebelum penggunaan. */
void classifier_init(void);

/** Push satu sampel ke window. Return 1 jika window penuh. */
int classifier_push_sample(const imu_sample_t *sample);

/** Klasifikasi window yang sudah penuh. Reset window setelahnya.
 *  Return ACTIVITY_UNKNOWN jika window belum penuh. */
activity_t classifier_classify(void);

/** Convenience: push + auto-classify saat penuh.
 *  Return ACTIVITY_UNKNOWN selama window belum penuh. */
activity_t classifier_update(const imu_sample_t *sample);

/** Ekstrak fitur accelerometer. Diekspos untuk debugging/tuning. */
void extract_features_accel(const imu_sample_t *buf, int n,
                            accel_features_t *feat);

/** Ekstrak fitur gyroscope. Diekspos untuk debugging/tuning. */
void extract_features_gyro(const imu_sample_t *buf, int n,
                           gyro_features_t *feat);

#ifdef __cplusplus
}
#endif

#endif /* __CLASSIFIER_H__ */

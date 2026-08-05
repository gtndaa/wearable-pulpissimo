#ifndef __CLASSIFIER_H__
#define __CLASSIFIER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t ax;   /**< Accelerometer X (raw LSB) */
    int16_t ay;   /**< Accelerometer Y (raw LSB) */
    int16_t az;   /**< Accelerometer Z (raw LSB) */
    int16_t gx;   /**< Gyroscope X (raw LSB) */
    int16_t gy;   /**< Gyroscope Y (raw LSB) */
    int16_t gz;   /**< Gyroscope Z (raw LSB) */
} imu_sample_t;

typedef enum {
    ACTIVITY_UNKNOWN = 0,  /**< Belum diklasifikasi */
    ACTIVITY_SIT,          /**< Duduk */
    ACTIVITY_STAND,        /**< Berdiri */
    ACTIVITY_LIE,          /**< Tiduran / rebahan (badan horizontal) */
    ACTIVITY_WALK,         /**< Berjalan (mencakup semua gerak aktif periodik) */
    ACTIVITY_FALL,         /**< Jatuh */
    ACTIVITY_COUNT         /**< Jumlah kelas (untuk array sizing) */
} activity_t;

/* ============================================================================
 * Konfigurasi Threshold & Parameter yang Bisa Di-tuning
 *
 * ============================================================================ */

/* ---- Window ---- */

/** Jumlah sampel per window. 32 @ 50 Hz = 640 ms.
 *  Memori: 32 × 12 bytes = 384 bytes. */
#define CLF_WINDOW_SIZE     32

/* ---- Temporal Smoothing (Majority Vote) ---- */

/** Jumlah window riwayat untuk majority vote.
 *  FALL bypass, langsung output tanpa vote (safety-critical).
 *  Latency terburuk: 3 × 640ms ≈ 2 detik. */
#define CLF_HISTORY_SIZE    3

/* ---- Level 1: Rest vs Active (SMA accelerometer, deviation-based) ---- */

/** SMA (deviasi dari rata-rata window, lihat classifier.c) mengukur
 *  intensitas gerak, bukan orientasi. Placeholder perlu di-tuning
 *  dengan data lapangan asli (pakai SHOW_FEATURES di test.c untuk amati
 *  nilai sma pada kondisi diam vs berjalan, lalu set threshold di
 *  tengah-tengah keduanya). Tidak ada kelas terpisah, window yang REST 
 *  langsung diklasifikasikan ke postur (SIT/STAND/LIE) via tilt. */
#define CLF_SMA_REST_THRESHOLD      60000L

/* ---- Level 2: Postur: Stand vs Sit (tilt ratio) ---- */

/** Tilt = (sum_az × 1024) / (|sum_ax|+|sum_ay|+|sum_az|+N).
 *  ~1024 = vertikal (berdiri), medium = condong (duduk),
 *  ~0 (atau negatif) = horizontal (tiduran/rebahan).
 *  Tergantung pemasangan sensor. */
#define CLF_TILT_STAND_THRESHOLD    700

/* ---- Level 2: Postur: Lie/Rebahan (tilt ratio, badan horizontal) ---- */

/** Jika |tilt_ratio| < ambang ini, sumbu Z sensor hampir tegak lurus
 *  terhadap gravitasi → badan horizontal (tiduran/rebahan), bukan duduk.
 *  Gunakan abs() karena orientasi rebahan bisa membuat tilt sedikit
 *  positif maupun negatif tergantung sisi tubuh yang menghadap ke bawah. */
#define CLF_TILT_LIE_THRESHOLD      250

/* ---- Level 3: Fall Detection (SVM accel + energy gyro) ---- */

/** SVM peak = max(ax²+ay²+az²). TANPA sqrt. uint32_t.
 *  800M ≈ ~1.7g impact. Memerlukan kedua kondisi (+ gyro). */
#define CLF_SVM_FALL_THRESHOLD          800000000UL

/** Energy gyro saat jatuh. Rotasi tak terkontrol >> gerak berjalan. */
#define CLF_GYRO_ENERGY_FALL_THRESHOLD  5000000L

/* ---- Level 4: Walk Detection (energy + ZCR gyro) ----

/** Energy = Σ((gx>>4)²+(gy>>4)²+(gz>>4)²). Right-shift 4 mencegah overflow.
 *  Diam: 26K–111K. Jalan: 108K–2.6M.
 *  Walk batas bawah 150K (di atas noise diam max 111K). */
#define CLF_GYRO_ENERGY_WALK_THRESHOLD  150000L

/** ZCR minimum = pola periodik (langkah). Diam: 0–2, jalan: 4–15. */
#define CLF_GYRO_ZCR_WALK_MIN           4

/* > **Catatan:** Threshold energy/mean_abs sudah dikalikan N (window size).
 *  Jika rata-rata per sampel yang diinginkan = X,set threshold = X × CLF_WINDOW_SIZE. */

/* ============================================================================
 * Feature Structures
 * ============================================================================ */

/** Fitur accelerometer: SMA (deviation-based), SVM peak, tilt ratio. */
typedef struct {
    int32_t  sma;          /**< Σ|ax-mean_ax|+|ay-mean_ay|+|az-mean_az| (deviasi) */
    uint32_t svm_max;      /**< max(ax²+ay²+az²), dari nilai RAW, tanpa sqrt */
    int32_t  tilt_ratio;   /**< (sum_az×1024)/(|sum_ax|+|sum_ay|+|sum_az|+N) */
} accel_features_t;

/** Fitur gyroscope: energy, ZCR, mean absolute. */
typedef struct {
    int32_t energy;        /**< Σ((gx>>4)²+(gy>>4)²+(gz>>4)²), tidak dibagi N */
    int32_t zcr;           /**< Zero-crossing count pada gx */
    int32_t mean_abs;      /**< Σ(|gx|+|gy|+|gz|), tidak dibagi N (untuk debugging) */
} gyro_features_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

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
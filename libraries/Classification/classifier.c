/*
 * Copyright (C) 2026 ICDeC
 *
 * Activity Classifier — Implementation
 *
 * Implementasi classifier aktivitas real-time menggunakan hierarchical
 * decision tree, terinspirasi dari:
 *
 *   - Karantonis et al. (2006), "Implementation of a Real-Time Human
 *     Movement Classifier Using a Triaxial Accelerometer"
 *     → Hierarchical binary decision tree, fitur SMA & SVM, tilt angle,
 *       trik multiply-threshold (bukan divide) untuk menghindari pembagian
 *
 *   - Bourke et al. (2007), "Evaluation of a threshold-based tri-axial
 *     accelerometer fall detection algorithm"
 *     → Single-threshold SVM untuk deteksi fall, dikonfirmasi dengan
 *       data gyro untuk mengurangi false positive
 *
 * PEMBAGIAN PERAN SENSOR:
 *   Accelerometer (ax, ay, az):
 *     - Postur/orientasi → tilt angle (perbandingan sumbu gravitasi)
 *     - Intensitas aktivitas → SMA (membedakan rest vs active)
 *     - Deteksi benturan → SVM peak (deteksi fall)
 *
 *   Gyroscope (gx, gy, gz):
 *     - Pola gerakan rotasional → energy (membedakan walk vs run)
 *     - Periodisitas langkah → ZCR (zero-crossing rate)
 *     - Konfirmasi fall → lonjakan energy rotasi saat jatuh
 *
 * OPTIMISASI UNTUK MCU:
 *   - Semua aritmetika integer/fixed-point, TANPA float/double
 *   - Tidak ada sqrt/exp/pembagian mahal
 *   - Trik Karantonis: bandingkan sum vs threshold×N, bukan mean vs threshold
 *   - Right-shift sebelum kuadrat untuk menghindari overflow
 *   - Total RAM < 500 bytes
 *
 * MODUL INI TIDAK PERNAH INCLUDE HEADER DRIVER SENSOR SPESIFIK.
 * Ia hanya bergantung pada imu_types.h (imu_sample_t, activity_t).
 */

#include "classifier.h"
#include "classifier_config.h"

/* ============================================================================
 * Sliding Window Buffer
 *
 * Buffer siklis berukuran CLF_WINDOW_SIZE untuk menyimpan sampel IMU.
 * Saat window_idx mencapai CLF_WINDOW_SIZE, window dianggap penuh dan
 * siap untuk feature extraction + klasifikasi.
 *
 * Setelah klasifikasi, window di-reset (window_idx = 0, window_full = 0)
 * untuk mulai mengumpulkan window baru. Ini adalah NON-OVERLAPPING window
 * (bukan sliding overlap) untuk menghemat CPU pada MCU kecil.
 *
 * Memori: CLF_WINDOW_SIZE × sizeof(imu_sample_t) = 32 × 12 = 384 bytes
 * ============================================================================ */

static imu_sample_t window_buf[CLF_WINDOW_SIZE];
static int window_idx;    /* Indeks tulis berikutnya (0 .. CLF_WINDOW_SIZE-1) */
static int window_full;   /* 1 jika window sudah penuh, 0 jika belum */

/* ============================================================================
 * Helper: Absolute value untuk int16_t dan int32_t
 *
 * Tidak pakai stdlib abs() untuk menghindari dependensi tambahan pada
 * beberapa toolchain embedded yang mungkin tidak punya stdlib lengkap.
 * ============================================================================ */

static inline int16_t abs16(int16_t x)
{
    return (x < 0) ? -x : x;
}

static inline int32_t abs32(int32_t x)
{
    return (x < 0) ? -x : x;
}

/* ============================================================================
 * Feature Extraction — Accelerometer
 *
 * Mengekstrak tiga fitur dari komponen accelerometer:
 *
 * 1. SMA (Signal Magnitude Area)
 *    = Σ(|ax| + |ay| + |az|) untuk seluruh window
 *    TIDAK dibagi N → threshold di classifier_config.h sudah dikalikan N
 *    (trik Karantonis: menghindari pembagian integer yang mahal)
 *
 *    Interpretasi:
 *    - Saat diam, SMA ≈ 1g × N (hanya komponen gravitasi statis)
 *    - Saat aktif, SMA naik karena akselerasi dinamis ditambahkan
 *
 * 2. SVM peak (Signal Vector Magnitude, dalam bentuk kuadrat)
 *    = max(ax² + ay² + az²) dalam window
 *    TANPA sqrt → threshold juga dalam bentuk kuadrat
 *    Dipakai untuk mendeteksi benturan tinggi (fall)
 *    Mirip pendekatan Bourke et al. (2007)
 *
 * 3. Tilt ratio (proxy sudut kemiringan, fixed-point ×1024)
 *    = (mean_az × 1024) / (|mean_ax| + |mean_ay| + |mean_az| + 1)
 *    Menggantikan arccos yang terlalu mahal untuk MCU ini
 *    - Nilai mendekati 1024 → sumbu Z dominan → sensor vertikal (berdiri)
 *    - Nilai mendekati 0 → sensor horizontal/miring (duduk/rebah)
 *    - +1 di penyebut untuk menghindari division by zero
 *
 * Overflow analysis:
 *   SMA: max |ax|+|ay|+|az| = 32767×3 = 98,301 per sampel
 *         × 32 sampel = 3,145,632 → aman int32_t
 *   SVM: max ax²+ay²+az² = 32767²×3 = 3,221,028,867 → MELEBIHI int32_t!
 *         Dipakai uint32_t (max 4,294,967,295) → aman
 *   Tilt: mean per sumbu max 32767, ×1024 = 33,553,408 → aman int32_t
 * ============================================================================ */

void extract_features_accel(const imu_sample_t *buf, int n,
                            accel_features_t *feat)
{
    int32_t sum_sma = 0;
    uint32_t max_svm = 0;
    int32_t sum_ax = 0, sum_ay = 0, sum_az = 0;
    int i;

    for (i = 0; i < n; i++) {
        int16_t ax = buf[i].ax;
        int16_t ay = buf[i].ay;
        int16_t az = buf[i].az;

        /* SMA: akumulasi |ax| + |ay| + |az| */
        sum_sma += (int32_t)abs16(ax) + (int32_t)abs16(ay) + (int32_t)abs16(az);

        /* SVM (kuadrat): ax² + ay² + az², simpan maximum */
        uint32_t svm = (uint32_t)((int32_t)ax * ax)
                     + (uint32_t)((int32_t)ay * ay)
                     + (uint32_t)((int32_t)az * az);
        if (svm > max_svm) {
            max_svm = svm;
        }

        /* Akumulasi untuk mean (tilt calculation) */
        sum_ax += (int32_t)ax;
        sum_ay += (int32_t)ay;
        sum_az += (int32_t)az;
    }

    feat->sma = sum_sma;
    feat->svm_max = max_svm;

    /* Tilt ratio: (mean_az × 1024) / (|mean_ax| + |mean_ay| + |mean_az| + 1)
     *
     * Kita pakai sum langsung (bukan mean = sum/N) karena faktor N
     * tereduksi di pembilang dan penyebut:
     *   (sum_az/N × 1024) / (|sum_ax/N| + |sum_ay/N| + |sum_az/N| + 1)
     * = (sum_az × 1024) / (|sum_ax| + |sum_ay| + |sum_az| + N)
     *
     * +N (bukan +1) di penyebut untuk menjaga konsistensi skala. */
    int32_t denom = abs32(sum_ax) + abs32(sum_ay) + abs32(sum_az) + (int32_t)n;
    feat->tilt_ratio = (sum_az * 1024) / denom;
}

/* ============================================================================
 * Feature Extraction — Gyroscope
 *
 * Mengekstrak tiga fitur dari komponen gyroscope:
 *
 * 1. Energy (kuadrat, scaled)
 *    = Σ((gx>>2)² + (gy>>2)² + (gz>>2)²) untuk seluruh window
 *    Right-shift 2 bit SEBELUM kuadrat:
 *      Tanpa shift: max = 32767² × 3 = 3,221,028,867 (> INT32_MAX per sampel!)
 *      Dengan shift: max = 8191² × 3 = 201,293,823 per sampel
 *                    × 32 sampel = 6,441,402,336 → masih > INT32_MAX!
 *    Solusi: shift 4 bit: max = 2047² × 3 = 12,573,027 per sampel
 *                         × 32 sampel = 402,336,864 → aman int32_t
 *    TRADE-OFF: kehilangan 4 bit resolusi bawah, tapi cukup untuk
 *    membedakan pola gerakan kasar (walk vs run vs fall)
 *
 * 2. ZCR (Zero-Crossing Rate)
 *    = jumlah perubahan tanda pada gx antar sampel berurutan
 *    Dipilih sumbu X karena biasanya paling dominan untuk rotasi
 *    pergelangan (wrist-worn sensor). Bisa disesuaikan.
 *    Walk/run → pola periodik → ZCR tinggi
 *    Diam/drift → ZCR rendah
 *
 * 3. Mean absolute
 *    = Σ(|gx| + |gy| + |gz|) untuk seluruh window
 *    TIDAK dibagi N (trik Karantonis)
 *    Dipakai sebagai secondary check: jika sangat rendah → sensor
 *    benar-benar diam, bukan hanya ZCR rendah karena drift konstan
 *
 * Overflow analysis (mean_abs):
 *   max |gx|+|gy|+|gz| = 32767×3 = 98,301 per sampel
 *   × 32 sampel = 3,145,632 → aman int32_t
 * ============================================================================ */

/** Jumlah bit right-shift sebelum kuadrat, untuk menghindari overflow. */
#define GYRO_ENERGY_SHIFT   4

void extract_features_gyro(const imu_sample_t *buf, int n,
                           gyro_features_t *feat)
{
    int32_t sum_energy = 0;
    int32_t zcr_count = 0;
    int32_t sum_abs = 0;
    int i;

    for (i = 0; i < n; i++) {
        int16_t gx = buf[i].gx;
        int16_t gy = buf[i].gy;
        int16_t gz = buf[i].gz;

        /* Energy: right-shift lalu kuadrat, akumulasi */
        int32_t gx_s = (int32_t)(gx >> GYRO_ENERGY_SHIFT);
        int32_t gy_s = (int32_t)(gy >> GYRO_ENERGY_SHIFT);
        int32_t gz_s = (int32_t)(gz >> GYRO_ENERGY_SHIFT);
        sum_energy += gx_s * gx_s + gy_s * gy_s + gz_s * gz_s;

        /* ZCR: deteksi perubahan tanda pada gx (sumbu X gyro) */
        if (i > 0) {
            int16_t prev_gx = buf[i - 1].gx;
            /* Zero-crossing terjadi saat tanda berubah:
             * (prev < 0 && curr >= 0) || (prev >= 0 && curr < 0)
             * Cara cepat: XOR bit tanda */
            if ((prev_gx ^ gx) < 0) {
                zcr_count++;
            }
        }

        /* Mean absolute: akumulasi |gx| + |gy| + |gz| */
        sum_abs += (int32_t)abs16(gx) + (int32_t)abs16(gy) + (int32_t)abs16(gz);
    }

    feat->energy = sum_energy;
    feat->zcr = zcr_count;
    feat->mean_abs = sum_abs;
}

/* ============================================================================
 * Hierarchical Decision Tree
 *
 * Struktur keputusan multi-level yang menggabungkan fitur dari
 * accelerometer DAN gyroscope:
 *
 * Level 1: REST vs ACTIVE
 *   Fitur: SMA accelerometer
 *   Alasan: SMA secara alami menangkap magnitude total akselerasi.
 *           Saat diam, hanya ada gravitasi statis (~1g).
 *           Saat bergerak, akselerasi dinamis menambah SMA secara signifikan.
 *
 * Level 2 (jika REST): SIT vs STAND
 *   Fitur: tilt ratio accelerometer
 *   Alasan: postur (duduk vs berdiri) terlihat dari orientasi gravitasi
 *           relatif terhadap sumbu sensor. Berdiri → sumbu Z dominan.
 *           Duduk → distribusi lebih merata atau sumbu lain dominan.
 *
 * Level 3 (jika ACTIVE): FALL check
 *   Fitur: SVM peak accelerometer + energy gyroscope
 *   Alasan: fall ditandai oleh KEDUA:
 *     - Benturan tinggi (SVM accelerometer sangat besar) — pendekatan Bourke
 *     - Rotasi cepat (energy gyro tinggi) — membedakan fall dari impact biasa
 *   Memerlukan kedua kondisi bersamaan untuk mengurangi false positive
 *
 * Level 4 (jika ACTIVE, bukan FALL): WALK vs RUN
 *   Fitur: energy gyroscope + ZCR gyroscope
 *   Alasan: gerakan periodik (jalan/lari) memiliki ZCR tinggi pada gyro.
 *           Intensitas gerakan (jalan pelan vs lari cepat) terlihat dari
 *           magnitude energy gyro.
 * ============================================================================ */

static activity_t decide(const accel_features_t *af, const gyro_features_t *gf)
{
    /* ---- Level 1: Rest vs Active ----
     * SMA < threshold → rest (gravitasi statis saja)
     * SMA >= threshold → active (ada akselerasi dinamis)
     *
     * Catatan: threshold sudah dikalikan N di classifier_config.h,
     * jadi kita bandingkan langsung dengan sum (BUKAN mean).
     * Ini menghilangkan satu operasi pembagian per klasifikasi. */
    if (af->sma < CLF_SMA_REST_THRESHOLD) {

        /* ---- Level 2: Sit vs Stand ----
         * tilt_ratio tinggi → sumbu Z dominan → berdiri
         * tilt_ratio rendah → condong/rebah → duduk
         *
         * Secondary check: jika gyro mean_abs sangat rendah,
         * perangkat benar-benar tidak bergerak → bisa STILL.
         * Tapi kita tetap bedakan berdasarkan postur. */
        if (gf->mean_abs < CLF_GYRO_MEAN_ABS_STILL_THRESHOLD) {
            /* Gyro hampir nol → benar-benar diam total */
            return ACTIVITY_STILL;
        }

        if (af->tilt_ratio > CLF_TILT_STAND_THRESHOLD) {
            return ACTIVITY_STAND;
        } else {
            return ACTIVITY_SIT;
        }
    }

    /* ---- Active branch ---- */

    /* ---- Level 3: Fall check ----
     * Memerlukan KEDUA kondisi:
     *   1. SVM peak sangat tinggi (benturan keras)
     *   2. Gyro energy tinggi (rotasi cepat saat jatuh)
     *
     * Dual-condition mengurangi false positive:
     *   - Tepukan keras pada sensor → SVM tinggi, tapi gyro energy rendah
     *   - Mengayunkan tangan cepat → gyro tinggi, tapi SVM mungkin tidak
     *     setinggi fall */
    if (af->svm_max > CLF_SVM_FALL_THRESHOLD &&
        gf->energy > CLF_GYRO_ENERGY_FALL_THRESHOLD) {
        return ACTIVITY_FALL;
    }

    /* ---- Level 4: Walk vs Run ----
     * ZCR minimum memastikan ada pola periodik (langkah kaki).
     * Energy gyro membedakan intensitas:
     *   - Energy tinggi → run (rotasi sendi lebih intens)
     *   - Energy sedang → walk */
    if (gf->zcr >= CLF_GYRO_ZCR_WALK_MIN &&
        gf->energy >= CLF_GYRO_ENERGY_WALK_THRESHOLD) {

        if (gf->energy >= CLF_GYRO_ENERGY_RUN_THRESHOLD) {
            return ACTIVITY_RUN;
        }
        return ACTIVITY_WALK;
    }

    /* Fallback: aktif tapi tidak cocok pola walk/run/fall.
     * Mungkin gerakan tangan acak, transisi, atau noise.
     * Dikategorikan STILL sebagai default konservatif. */
    return ACTIVITY_STILL;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void classifier_init(void)
{
    window_idx = 0;
    window_full = 0;
    /* Tidak perlu memset window_buf — akan di-overwrite saat diisi.
     * Menghemat siklus CPU yang berharga pada MCU kecil. */
}

int classifier_push_sample(const imu_sample_t *sample)
{
    if (window_full) {
        /* Window sudah penuh tapi belum di-classify.
         * Abaikan sampel baru untuk menghindari data loss di window saat ini.
         * Caller seharusnya memanggil classifier_classify() dulu. */
        return 1;
    }

    window_buf[window_idx] = *sample;  /* Copy 12 bytes */
    window_idx++;

    if (window_idx >= CLF_WINDOW_SIZE) {
        window_full = 1;
        return 1;
    }
    return 0;
}

activity_t classifier_classify(void)
{
    if (!window_full) {
        return ACTIVITY_UNKNOWN;
    }

    /* Ekstrak fitur dari kedua sensor secara terpisah */
    accel_features_t af;
    gyro_features_t gf;

    extract_features_accel(window_buf, CLF_WINDOW_SIZE, &af);
    extract_features_gyro(window_buf, CLF_WINDOW_SIZE, &gf);

    /* Jalankan hierarchical decision tree */
    activity_t result = decide(&af, &gf);

    /* Reset window untuk mulai mengumpulkan sampel baru */
    window_idx = 0;
    window_full = 0;

    return result;
}

activity_t classifier_update(const imu_sample_t *sample)
{
    if (classifier_push_sample(sample)) {
        return classifier_classify();
    }
    return ACTIVITY_UNKNOWN;
}

/*
 * Copyright (C) 2026 ICDeC
 *
 * Activity Classifier — Implementation
 *
 * Hierarchical decision tree + majority vote temporal smoothing.
 * Sensor-agnostic: hanya bergantung pada imu_types.h.
 * Integer-only arithmetic, tanpa float/double.
 *
 * Lihat README.md untuk penjelasan lengkap decision tree,
 * feature extraction, overflow analysis, dan data lapangan.
 *
 * Referensi:
 *   - Karantonis et al. (2006) — decision tree, trik Karantonis
 *   - Bourke et al. (2007) — SVM fall detection
 */

#include "classifier.h"
#include "classifier_config.h"

/* ---- Sliding Window Buffer ---- */

static imu_sample_t window_buf[CLF_WINDOW_SIZE];
static int window_idx;
static int window_full;

/* ---- Majority Vote History ---- */

static uint8_t vote_history[CLF_HISTORY_SIZE];
static int vote_idx;
static int vote_count;

/* ---- Helpers ---- */

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
 * SMA:      Σ(|ax|+|ay|+|az|). Tidak dibagi N (trik Karantonis).
 * SVM peak: max(ax²+ay²+az²). Tanpa sqrt. uint32_t.
 * Tilt:     (sum_az × 1024) / (|sum_ax|+|sum_ay|+|sum_az|+N). ×1024 fixed-pt.
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

        sum_sma += (int32_t)abs16(ax) + (int32_t)abs16(ay) + (int32_t)abs16(az);

        uint32_t svm = (uint32_t)((int32_t)ax * ax)
                     + (uint32_t)((int32_t)ay * ay)
                     + (uint32_t)((int32_t)az * az);
        if (svm > max_svm) {
            max_svm = svm;
        }

        sum_ax += (int32_t)ax;
        sum_ay += (int32_t)ay;
        sum_az += (int32_t)az;
    }

    feat->sma = sum_sma;
    feat->svm_max = max_svm;

    /* Tilt: faktor N tereduksi di pembilang/penyebut, +N di penyebut. */
    int32_t denom = abs32(sum_ax) + abs32(sum_ay) + abs32(sum_az) + (int32_t)n;
    feat->tilt_ratio = (sum_az * 1024) / denom;
}

/* ============================================================================
 * Feature Extraction — Gyroscope
 *
 * Energy:   Σ((gx>>4)²+(gy>>4)²+(gz>>4)²). Shift 4 mencegah overflow.
 * ZCR:      Perubahan tanda gx (XOR bit tanda).
 * Mean abs: Σ(|gx|+|gy|+|gz|). Tidak dibagi N.
 * ============================================================================ */

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

        int32_t gx_s = (int32_t)(gx >> GYRO_ENERGY_SHIFT);
        int32_t gy_s = (int32_t)(gy >> GYRO_ENERGY_SHIFT);
        int32_t gz_s = (int32_t)(gz >> GYRO_ENERGY_SHIFT);
        sum_energy += gx_s * gx_s + gy_s * gy_s + gz_s * gz_s;

        if (i > 0) {
            int16_t prev_gx = buf[i - 1].gx;
            if ((prev_gx ^ gx) < 0) {
                zcr_count++;
            }
        }

        sum_abs += (int32_t)abs16(gx) + (int32_t)abs16(gy) + (int32_t)abs16(gz);
    }

    feat->energy = sum_energy;
    feat->zcr = zcr_count;
    feat->mean_abs = sum_abs;
}

/* ============================================================================
 * Hierarchical Decision Tree
 *
 * L1: SMA → rest/active
 * L2: mean_abs → still | tilt → sit/stand/lie
 * L3: SVM + energy → fall
 * L4: ZCR + energy → walk/run
 * ============================================================================ */

static activity_t decide(const accel_features_t *af, const gyro_features_t *gf)
{
    /* L1: Rest vs Active */
    if (af->sma < CLF_SMA_REST_THRESHOLD) {

        /* L2: Still check */
        if (gf->mean_abs < CLF_GYRO_MEAN_ABS_STILL_THRESHOLD) {
            return ACTIVITY_STILL;
        }

        /* L2: Postur — Stand vs Sit vs Lie (tilt ratio) */
        if (af->tilt_ratio > CLF_TILT_STAND_THRESHOLD) {
            return ACTIVITY_STAND;
        } else if (abs32(af->tilt_ratio) < CLF_TILT_LIE_THRESHOLD) {
            /* Sumbu Z ~tegak lurus gravitasi → badan horizontal */
            return ACTIVITY_LIE;
        } else {
            return ACTIVITY_SIT;
        }
    }

    /* L3: Fall (dual-condition: SVM + gyro) */
    if (af->svm_max > CLF_SVM_FALL_THRESHOLD &&
        gf->energy > CLF_GYRO_ENERGY_FALL_THRESHOLD) {
        return ACTIVITY_FALL;
    }

    /* L4: Walk vs Run (ZCR + energy) */
    if (gf->zcr >= CLF_GYRO_ZCR_WALK_MIN &&
        gf->energy >= CLF_GYRO_ENERGY_WALK_THRESHOLD) {

        if (gf->energy >= CLF_GYRO_ENERGY_RUN_THRESHOLD) {
            return ACTIVITY_RUN;
        }
        return ACTIVITY_WALK;
    }

    /* Fallback: aktif tapi bukan pola walk/run/fall */
    return ACTIVITY_STILL;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void classifier_init(void)
{
    window_idx = 0;
    window_full = 0;
    vote_idx = 0;
    vote_count = 0;
}

int classifier_push_sample(const imu_sample_t *sample)
{
    if (window_full) {
        return 1;
    }

    window_buf[window_idx] = *sample;
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

    accel_features_t af;
    gyro_features_t gf;

    extract_features_accel(window_buf, CLF_WINDOW_SIZE, &af);
    extract_features_gyro(window_buf, CLF_WINDOW_SIZE, &gf);

    activity_t raw_result = decide(&af, &gf);

    window_idx = 0;
    window_full = 0;

    /* FALL bypass — langsung output, reset riwayat (safety-critical) */
    if (raw_result == ACTIVITY_FALL) {
        vote_idx = 0;
        vote_count = 0;
        return ACTIVITY_FALL;
    }

    /* Simpan hasil ke ring buffer */
    vote_history[vote_idx] = (uint8_t)raw_result;
    vote_idx = (vote_idx + 1) % CLF_HISTORY_SIZE;
    if (vote_count < CLF_HISTORY_SIZE) {
        vote_count++;
    }

    /* Majority vote: kelas paling sering menang.
     * Saat seri, >= memberikan bias ke kelas dengan indeks lebih tinggi. */
    int counts[ACTIVITY_COUNT] = {0};
    int i;
    for (i = 0; i < vote_count; i++) {
        uint8_t act = vote_history[i];
        if (act < ACTIVITY_COUNT) {
            counts[act]++;
        }
    }

    activity_t winner = raw_result;
    int max_count = 0;
    for (i = 1; i < ACTIVITY_COUNT; i++) {
        if (counts[i] >= max_count) {
            max_count = counts[i];
            winner = (activity_t)i;
        }
    }

    return winner;
}

activity_t classifier_update(const imu_sample_t *sample)
{
    if (classifier_push_sample(sample)) {
        return classifier_classify();
    }
    return ACTIVITY_UNKNOWN;
}

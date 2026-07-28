/*
 * Copyright (C) 2026 ICDeC
 *
 * Classifier Configuration — Tunable Thresholds & Parameters
 *
 * SATU-SATUNYA tempat untuk mengubah parameter classifier.
 * Ubah nilai di sini, recompile — logika classifier tidak perlu disentuh.
 *
 * Semua threshold bekerja pada skala RAW (int16_t register sensor).
 * Jika sensor range berubah (±2g → ±4g), threshold HARUS di-tuning ulang.
 *
 * Lihat README.md untuk panduan tuning, data lapangan, dan penjelasan
 * decision tree lengkap.
 */

#ifndef __CLASSIFIER_CONFIG_H__
#define __CLASSIFIER_CONFIG_H__

/* ---- Window ---- */

/** Jumlah sampel per window. 32 @ 50 Hz = 640 ms.
 *  Memori: 32 × 12 bytes = 384 bytes. */
#define CLF_WINDOW_SIZE     32

/* ---- Temporal Smoothing (Majority Vote) ---- */

/** Jumlah window riwayat untuk majority vote.
 *  FALL bypass — langsung output tanpa vote (safety-critical).
 *  Latency terburuk: 3 × 640ms ≈ 2 detik. */
#define CLF_HISTORY_SIZE    3

/* ---- Level 1: Rest vs Active (SMA accelerometer) ---- */

/** SMA = Σ(|ax|+|ay|+|az|), TIDAK dibagi N (trik Karantonis).
 *  Diam: ~588K–612K. Jalan: ~603K–739K. Titik tengah: 612K. */
#define CLF_SMA_REST_THRESHOLD      612000L

/* ---- Level 2: Postur — Sit vs Stand (tilt ratio) ---- */

/** Tilt = (sum_az × 1024) / (|sum_ax|+|sum_ay|+|sum_az|+N).
 *  ~1024 = vertikal (berdiri), ~0 = horizontal (duduk).
 *  Tergantung pemasangan sensor. */
#define CLF_TILT_STAND_THRESHOLD    700

/* ---- Level 2: Still check (gyro mean_abs) ---- */

/** Mean abs gyro = Σ(|gx|+|gy|+|gz|), TIDAK dibagi N.
 *  < 50K → benar-benar diam (STILL), bukan hanya SMA rendah. */
#define CLF_GYRO_MEAN_ABS_STILL_THRESHOLD   50000L

/* ---- Level 3: Fall Detection (SVM accel + energy gyro) ---- */

/** SVM peak = max(ax²+ay²+az²). TANPA sqrt. uint32_t.
 *  800M ≈ ~1.7g impact. Memerlukan KEDUA kondisi (+ gyro). */
#define CLF_SVM_FALL_THRESHOLD          800000000UL

/** Energy gyro saat jatuh. Rotasi tak terkontrol >> walk/run. */
#define CLF_GYRO_ENERGY_FALL_THRESHOLD  5000000L

/* ---- Level 4: Walk vs Run (energy + ZCR gyro) ---- */

/** Energy = Σ((gx>>4)²+(gy>>4)²+(gz>>4)²). Right-shift 4 mencegah overflow.
 *  Diam: 26K–111K. Jalan: 108K–2.6M. Lari: 5M+.
 *  Walk batas bawah 150K (di atas noise diam max 111K).
 *  Run batas bawah 5M (konservatif, belum ada data lari asli). */
#define CLF_GYRO_ENERGY_WALK_THRESHOLD  150000L
#define CLF_GYRO_ENERGY_RUN_THRESHOLD   5000000L

/** ZCR minimum = pola periodik (langkah). Diam: 0–2, jalan: 4–15. */
#define CLF_GYRO_ZCR_WALK_MIN           4

#endif /* __CLASSIFIER_CONFIG_H__ */

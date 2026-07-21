/*
 * Copyright (C) 2026 ICDeC
 *
 * Classifier Configuration — Tunable Thresholds & Parameters
 *
 * File ini adalah SATU-SATUNYA tempat semua parameter yang bisa di-tuning
 * untuk classifier. Tujuannya: setelah pengumpulan data lapangan dan
 * analisis offline, cukup ubah nilai-nilai di sini lalu recompile —
 * tidak perlu menyentuh logika classifier sama sekali.
 *
 * PANDUAN TUNING THRESHOLD:
 * ============================================================================
 *
 * 1. KUMPULKAN DATA LAPANGAN
 *    - Pakai adapter sensor (mis. mpu6050_to_imu_sample()) untuk merekam
 *      imu_sample_t selama minimal 1 menit per aktivitas target
 *    - Kirim data via UART/log ke PC untuk analisis offline
 *
 * 2. EKSTRAK FITUR OFFLINE
 *    - Jalankan extract_features_accel() dan extract_features_gyro() pada
 *      setiap window dari data rekaman (bisa pakai test harness yang sama)
 *    - Catat distribusi setiap fitur per kelas aktivitas
 *
 * 3. TENTUKAN THRESHOLD
 *    - Plot distribusi fitur, cari batas yang memisahkan kelas terbaik
 *    - Opsional: train sklearn DecisionTreeClassifier(max_depth=4) dari
 *      fitur integer, ekstrak threshold dari tree
 *    - PENTING: threshold SMA dan mean_abs di sini sudah DIKALIKAN N
 *      (window size) sesuai trik Karantonis — jadi jika rata-rata SMA
 *      per sampel yang diinginkan = X, set threshold = X * CLF_WINDOW_SIZE
 *
 * 4. HARDCODE & VALIDASI
 *    - Masukkan threshold ke file ini, recompile firmware
 *    - Uji real-time di device, iterasi jika perlu
 *
 * ============================================================================
 *
 * CATATAN TENTANG SKALA THRESHOLD:
 *   Semua threshold bekerja pada skala RAW (int16_t dari register sensor).
 *   Jika sensor range berubah (mis. dari ±2g ke ±4g), threshold HARUS
 *   di-tuning ulang karena skala LSB/g berubah.
 *
 *   Contoh untuk MPU-6050 ±2g (16384 LSB/g):
 *   - 1g gravitasi ≈ 16384 LSB
 *   - SMA saat diam ≈ 16384 × N (hanya komponen gravitasi)
 *   - SVM saat diam ≈ 16384² ≈ 268,435,456
 */

#ifndef __CLASSIFIER_CONFIG_H__
#define __CLASSIFIER_CONFIG_H__

/* ============================================================================
 * Window Parameters
 * ============================================================================ */

/**
 * Jumlah sampel per sliding window.
 *
 * Trade-off:
 *   - Lebih besar → klasifikasi lebih stabil, tapi latency lebih tinggi
 *     dan RAM lebih banyak
 *   - Lebih kecil → responsif, tapi lebih noise/tidak stabil
 *
 * Pada 50 Hz sampling rate:
 *   32 sampel = 640 ms per window (cukup untuk menangkap ~1 langkah jalan)
 *
 * Memori: 32 × 12 bytes = 384 bytes
 */
#define CLF_WINDOW_SIZE     32

/* ============================================================================
 * Level 1: Rest vs Active (menggunakan SMA accelerometer)
 *
 * SMA (Signal Magnitude Area) = Σ(|ax| + |ay| + |az|) untuk seluruh window.
 * TIDAK dibagi N — threshold sudah dikalikan N (trik Karantonis).
 *
 * Saat diam (hanya gravitasi), SMA ≈ 1g × N.
 * Saat aktif (jalan/lari), SMA jauh lebih tinggi karena akselerasi dinamis.
 *
 * Placeholder di bawah diasumsikan untuk MPU-6050 ±2g (16384 LSB/g):
 *   SMA diam ≈ 16384 × 32 = 524,288
 *   Threshold dipasang sedikit di atas untuk margin keamanan.
 * ============================================================================ */

#define CLF_SMA_REST_THRESHOLD      600000L

/* ============================================================================
 * Level 2: Postur — Sit vs Stand (menggunakan tilt ratio accelerometer)
 *
 * Tilt ratio = (mean_az × 1024) / (|mean_ax| + |mean_ay| + |mean_az| + 1)
 *
 * Nilai mendekati 1024 → sensor vertikal (sumbu Z dominan) → BERDIRI
 * Nilai mendekati 0    → sensor horizontal/miring → DUDUK
 *
 * Catatan: orientasi sumbu tergantung pemasangan sensor di wearable.
 * Threshold ini HARUS di-tuning sesuai posisi pemasangan sensor.
 * ============================================================================ */

#define CLF_TILT_STAND_THRESHOLD    700

/* ============================================================================
 * Level 3: Jenis Gerakan Aktif — Walk vs Run (menggunakan fitur gyro)
 *
 * Gyro energy = Σ((gx>>2)² + (gy>>2)² + (gz>>2)²) untuk seluruh window.
 * Right-shift 2 bit SEBELUM kuadrat untuk menghindari overflow int32_t.
 *
 * Gyro ZCR (Zero-Crossing Rate) = jumlah perubahan tanda pada gx antar
 * sampel berurutan. Aktivitas berjalan/berlari memiliki ZCR yang lebih
 * tinggi daripada gerakan acak.
 *
 * CLF_GYRO_ZCR_WALK_MIN: ZCR minimum untuk dianggap gerakan periodik
 *   (jalan/lari). Jika ZCR terlalu rendah → bukan pola langkah.
 *
 * CLF_GYRO_ENERGY_WALK_THRESHOLD: batas bawah energy gyro untuk WALK.
 * CLF_GYRO_ENERGY_RUN_THRESHOLD:  batas bawah energy gyro untuk RUN.
 *   Jika energy > RUN_THRESHOLD → RUN, else → WALK.
 * ============================================================================ */

#define CLF_GYRO_ENERGY_WALK_THRESHOLD   50000L
#define CLF_GYRO_ENERGY_RUN_THRESHOLD   200000L
#define CLF_GYRO_ZCR_WALK_MIN            4

/* ============================================================================
 * Level 4: Fall Detection (menggunakan SVM accel + energy gyro)
 *
 * Deteksi jatuh memerlukan DUA kondisi terpenuhi bersamaan:
 *   1. SVM peak accelerometer sangat tinggi (benturan keras)
 *   2. Energy gyro tinggi (rotasi cepat saat jatuh)
 *
 * Pendekatan mirip Bourke et al. (2007) untuk SVM, ditambah konfirmasi
 * dari gyro untuk mengurangi false positive.
 *
 * CLF_SVM_FALL_THRESHOLD: threshold untuk SVM peak (ax² + ay² + az²).
 *   TANPA sqrt — threshold juga dalam bentuk kuadrat.
 *   Untuk MPU-6050 ±2g: 2g impact ≈ (2×16384)² ≈ 1,073,741,824
 *   Fall biasanya > 3g → threshold ≈ (3×16384)² ≈ 2,415,919,104
 *   Catatan: ini bisa melebihi int32_t, jadi dipakai uint32_t.
 *
 * CLF_GYRO_ENERGY_FALL_THRESHOLD: lonjakan energy gyro saat jatuh.
 *   Biasanya jauh lebih tinggi dari walk/run karena rotasi tidak terkontrol.
 * ============================================================================ */

#define CLF_SVM_FALL_THRESHOLD          800000000UL
#define CLF_GYRO_ENERGY_FALL_THRESHOLD  500000L

/* ============================================================================
 * Mean Absolute Gyro — Tambahan untuk filtering noise
 *
 * Mean absolute gyro = Σ(|gx| + |gy| + |gz|) untuk seluruh window.
 * TIDAK dibagi N (trik Karantonis).
 *
 * Digunakan sebagai secondary check: jika mean_abs sangat rendah,
 * sensor benar-benar diam (bukan hanya ZCR rendah karena drift konstan).
 * ============================================================================ */

#define CLF_GYRO_MEAN_ABS_STILL_THRESHOLD   10000L

#endif /* __CLASSIFIER_CONFIG_H__ */

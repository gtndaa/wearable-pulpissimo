# Activity Classifier — IMU Fusion (Gyro + Accel)

Library klasifikasi aktivitas real-time berbasis fusi data gyroscope +
accelerometer untuk platform PULPissimo (RISC-V, tanpa FPU, 64 kB unified
memory).

## Arsitektur Pipeline

```
 Sensor Driver          Adapter              Classifier Module
 ┌─────────────┐    ┌──────────────┐    ┌─────────────────────────────────┐
 │  MPU-6050   │    │ mpu6050_to_  │    │                                 │
 │  L3G4200D   │───>│ imu_sample() │───>│  imu_sample_t                   │
 │  ...        │    │              │    │       │                         │
 └─────────────┘    └──────────────┘    │       ▼                         │
                                        │  Sliding Window (32 samples)    │
                                        │       │                         │
                                        │       ▼                         │
                                        │  Feature Extraction             │
                                        │  ┌──────────┬──────────┐        │
                                        │  │  Accel   │  Gyro    │        │
                                        │  │  SMA     │  Energy  │        │
                                        │  │  SVM     │  ZCR     │        │
                                        │  │  Tilt    │  MeanAbs │        │
                                        │  └────┬─────┴────┬─────┘        │
                                        │       │          │              │
                                        │       ▼          ▼              │
                                        │  Decision Tree (fusi)           │
                                        │       │                         │
                                        │       ▼                         │
                                        │  Majority Vote (smoothing)      │
                                        │       │                         │
                                        │       ▼                         │
                                        │  activity_t (output)            │
                                        └─────────────────────────────────┘
```

## Kelas Aktivitas

| Kode | Aktivitas | Fitur Dominan |
|------|-----------|---------------|
| `ACTIVITY_STILL` | Diam total | Gyro mean_abs sangat rendah |
| `ACTIVITY_SIT` | Duduk | SMA rendah + tilt ratio rendah |
| `ACTIVITY_STAND` | Berdiri | SMA rendah + tilt ratio tinggi |
| `ACTIVITY_WALK` | Berjalan | SMA tinggi + gyro energy sedang + ZCR tinggi |
| `ACTIVITY_RUN` | Berlari | SMA tinggi + gyro energy tinggi + ZCR tinggi |
| `ACTIVITY_FALL` | Jatuh | SVM peak sangat tinggi + gyro energy spike |

## Decision Tree

Struktur keputusan hierarki multi-level yang menggabungkan fitur
accelerometer dan gyroscope:

```
                          ┌─────────────┐
                          │  SMA accel  │
                          └──────┬──────┘
                     < 612K     │     ≥ 612K
                   ┌────────────┴────────────┐
                   │                         │
            ┌──────┴──────┐           ┌──────┴──────┐
            │  Gyro       │           │  SVM peak + │
            │  mean_abs   │           │  Gyro energy│
            └──────┬──────┘           └──────┬──────┘
       < 50K       │    ≥ 50K     FALL?      │     Tidak
          ┌────────┴────────┐     ┌──────────┴──────────┐
          │                 │     │                     │
       STILL          ┌────┴────┐│              ┌──────┴──────┐
                      │  Tilt   ││              │ Gyro energy │
                      │  ratio  ││              │ + ZCR       │
                      └────┬────┘│              └──────┬──────┘
                 > 700 │  ≤ 700  │       Walk?         │    Run?
                  ┌────┴────┐   FALL      ┌────────────┴────────┐
               STAND      SIT            WALK                 RUN
```

**Level 1 — Rest vs Active:**
SMA accelerometer memisahkan keadaan diam (hanya gravitasi ~1g) dari
keadaan bergerak (akselerasi dinamis menambah magnitude total).

**Level 2 — Postur (jika REST):**
Tilt ratio menentukan orientasi gravitasi relatif terhadap sumbu sensor.
Sumbu Z dominan → berdiri (tegak). Distribusi merata → duduk (condong).
Gyro mean_abs sangat rendah → diam total (STILL, tanpa micro-sway).

**Level 3 — Fall Detection (jika ACTIVE):**
Memerlukan **dua** kondisi bersamaan: SVM peak tinggi (benturan keras)
**dan** gyro energy tinggi (rotasi tak terkontrol). Dual-condition
mengurangi false positive — tepukan keras pada sensor memiliki SVM tinggi
tapi gyro rendah; mengayunkan tangan cepat memiliki gyro tinggi tapi SVM
tidak setinggi fall.

**Level 4 — Walk vs Run (jika ACTIVE, bukan FALL):**
ZCR minimum memastikan ada pola periodik (langkah kaki). Gyro energy
membedakan intensitas rotasi sendi — energi sedang = walk, tinggi = run.

## Temporal Smoothing (Majority Vote)

Untuk menghindari klasifikasi yang lompat-lompat antar kelas akibat noise
sesaat, digunakan mekanisme **majority vote**:

- Menyimpan riwayat `CLF_HISTORY_SIZE` (default: 3) klasifikasi terakhir
- Hasil final = kelas yang **paling sering muncul** dalam riwayat
- Jika seri, kelas yang paling baru memiliki sedikit keuntungan
- **PENGECUALIAN:** `ACTIVITY_FALL` **bypass** majority vote — langsung
  dilaporkan tanpa delay karena bersifat safety-critical. Fall juga
  me-reset seluruh riwayat vote.

**Trade-off:** Dengan window 640ms dan history 3, latency terburuk untuk
berpindah kelas = 3 × 640ms ≈ **2 detik**. Cukup responsif untuk deteksi
aktivitas manusia, tetapi cukup stabil untuk menghindari flicker.

## Feature Extraction

### Accelerometer Features

| Fitur | Formula | Interpretasi |
|-------|---------|--------------|
| **SMA** | `Σ(\|ax\| + \|ay\| + \|az\|)` | Magnitude total. TIDAK dibagi N (trik Karantonis — threshold dikalikan N, menghindari divisi). |
| **SVM peak** | `max(ax² + ay² + az²)` | Benturan tertinggi dalam window. TANPA sqrt — threshold juga dikuadratkan. Menggunakan `uint32_t` karena 32767² × 3 > INT32_MAX. |
| **Tilt ratio** | `(sum_az × 1024) / (\|sum_ax\| + \|sum_ay\| + \|sum_az\| + N)` | Proxy sudut kemiringan (fixed-point ×1024). Menggantikan arccos yang terlalu mahal untuk MCU. ~1024 = vertikal, ~0 = horizontal. |

### Gyroscope Features

| Fitur | Formula | Interpretasi |
|-------|---------|--------------|
| **Energy** | `Σ((gx>>4)² + (gy>>4)² + (gz>>4)²)` | Magnitude rotasi total. Right-shift 4 bit sebelum kuadrat menghindari overflow (tanpa shift: 32767² × 3 × 32 > INT32_MAX). Trade-off: kehilangan 4 bit resolusi bawah. |
| **ZCR** | Jumlah perubahan tanda `gx` | Zero-crossing rate pada sumbu X gyro. Gerakan periodik (langkah) → ZCR tinggi. Deteksi cepat via XOR bit tanda. |
| **Mean abs** | `Σ(\|gx\| + \|gy\| + \|gz\|)` | Secondary check. TIDAK dibagi N. Sangat rendah → sensor benar-benar diam (bukan ZCR rendah karena drift konstan). |

### Overflow Analysis

| Fitur | Max per sampel | × 32 window | Tipe |
|-------|----------------|-------------|------|
| SMA | 32767 × 3 = 98,301 | 3,145,632 | `int32_t` ✓ |
| SVM | 32767² × 3 = 3,221,028,867 | (per sampel) | `uint32_t` ✓ |
| Tilt | 32767 × 1024 = 33,553,408 | (per window) | `int32_t` ✓ |
| Energy | (32767>>4)² × 3 = 12,573,027 | 402,336,864 | `int32_t` ✓ |
| Mean abs | 32767 × 3 = 98,301 | 3,145,632 | `int32_t` ✓ |

## Data Lapangan & Threshold

Threshold dikalibrasi dari data lapangan MPU-6050 ±2g/±250dps pada PULPissimo
FPGA. Semua threshold bekerja pada skala **raw** (int16_t dari register sensor).
Jika sensor range berubah (mis. ±2g → ±4g), threshold **harus** di-tuning ulang.

### Distribusi Fitur per Aktivitas

Data dari dua ronde pengujian hardware (total 40 window, 1280 sampel):

| Aktivitas | SMA | Energy | ZCR | MeanAbs |
|-----------|-----|--------|-----|---------|
| Diam total | 588K–612K | 26K–111K | 0–2 | 17K–76K |
| Jalan ringan | 603K–659K | 108K–1.4M | 4–15 | 33K–167K |
| Jalan kuat | 673K–739K | 1.5M–2.6M | 7–11 | 151K–300K |
| Lari | 694K–739K | 6.0M–30.9M | 9–10 | 273K–621K |
| Jatuh | 638K | 11.1M | 8 | 179K |

### Threshold Saat Ini

| Parameter | Nilai | Level | Keterangan |
|-----------|-------|-------|------------|
| `CLF_SMA_REST_THRESHOLD` | 612,000 | L1 | Titik tengah diam (598K) vs jalan (628K) |
| `CLF_TILT_STAND_THRESHOLD` | 700 | L2 | > 700 = berdiri (Z dominan) |
| `CLF_GYRO_MEAN_ABS_STILL_THRESHOLD` | 50,000 | L2 | < 50K = benar-benar diam |
| `CLF_GYRO_ENERGY_WALK_THRESHOLD` | 150,000 | L4 | Di atas noise diam (max 111K) |
| `CLF_GYRO_ENERGY_RUN_THRESHOLD` | 5,000,000 | L4 | Di atas jalan kuat (max ~2.6M) |
| `CLF_GYRO_ZCR_WALK_MIN` | 4 | L4 | Minimum periodisitas langkah |
| `CLF_SVM_FALL_THRESHOLD` | 800,000,000 | L3 | ~1.7g impact (uint32_t) |
| `CLF_GYRO_ENERGY_FALL_THRESHOLD` | 5,000,000 | L3 | Rotasi tak terkontrol |
| `CLF_HISTORY_SIZE` | 3 | Vote | Jumlah window untuk majority vote |

> **Catatan:** Threshold SMA dan mean_abs sudah **dikalikan N** (window size)
> sesuai trik Karantonis. Jika rata-rata per sampel yang diinginkan = X,
> set threshold = X × `CLF_WINDOW_SIZE`.

## Footprint Memori

| Komponen | Bytes | Catatan |
|----------|-------|---------|
| `window_buf[32]` | 384 | 32 × 12 byte (`imu_sample_t`) |
| `accel_features_t` | 12 | 3 × int32_t |
| `gyro_features_t` | 12 | 3 × int32_t |
| `vote_history[3]` | 3 | Ring buffer majority vote |
| Control variables | 16 | window_idx, window_full, vote_idx, vote_count |
| Stack (lokal) | ~64 | Akumulator di feature extraction |
| **Total RAM** | **~491** | **< 0.77% dari 64 kB** |

Code size (ROM/flash) diestimasi ~2–3 kB.

## Optimisasi untuk MCU

- **Integer-only arithmetic** — tidak ada `float`, `double`, `sqrt`, `exp`
- **Trik Karantonis** — bandingkan `sum` vs `threshold×N`, bukan
  `mean` vs `threshold`. Menghindari satu divisi per klasifikasi.
- **Right-shift sebelum kuadrat** — mencegah overflow tanpa perlu 64-bit
- **XOR bit tanda** — deteksi zero-crossing tanpa branch/compare
- **Tanpa memset** — window buffer tidak di-nol-kan saat init karena
  akan di-overwrite saat diisi
- **Zero dependencies** — tidak ada `#include` ke hardware/sensor header

## Struktur File

```
Classification/
├── imu_types.h            Sensor abstraction (imu_sample_t, activity_t)
├── classifier_config.h    Semua threshold & parameter (tuning hub)
├── classifier.h           Public API + feature struct definitions
├── classifier.c           Implementasi (windowing + features + tree + vote)
├── test/
│   ├── test_classifier.c     Unit test (host GCC, mock data, tanpa hardware)
│   ├── test_classifier_hw.c  Hardware test (FPGA + MPU-6050, data asli)
│   └── Makefile              Build untuk host unit test + FPGA test
└── README.md

sensors/mpu6050/
└── mpu6050_adapter.h      Contoh adapter (BUKAN bagian classifier)
```

## Quick Start

### Unit Test (tanpa hardware)

```bash
cd libraries/Classification/test
make unit          # Build + jalankan semua 14 test
make unit_clean    # Bersihkan binary
```

### Hardware Test (FPGA + MPU-6050)

```bash
cd libraries/Classification/test
make all                     # Build untuk FPGA
make run platform=fpga       # Run di board
```

Hardware test menjalankan 4 tahap:
1. **TEST 1–2:** Init MPU-6050 (I2C, WHO_AM_I, wake, config)
2. **TEST 3:** Adapter sanity check — accel magnitude ~1g saat diam
3. **TEST 4:** Single window classification — mencetak semua fitur +
   threshold berdampingan (untuk tuning)
4. **Continuous:** Klasifikasi real-time 20 window dengan tabel fitur

### Integrasi dengan MPU-6050

```c
#include "mpu6050.h"
#include "mpu6050_adapter.h"
#include "classifier.h"

// Setelah mpu6050_init():
classifier_init();
while (1) {
    imu_sample_t sample;
    if (mpu6050_to_imu_sample(&sample) == MPU6050_OK) {
        activity_t act = classifier_update(&sample);
        if (act != ACTIVITY_UNKNOWN) {
            // act = STILL, SIT, STAND, WALK, RUN, atau FALL
        }
    }
    // delay sesuai sampling rate (~20ms untuk 50 Hz)
}
```

### Integrasi dengan Sensor Lain

Buat adapter baru yang mengisi `imu_sample_t`:

```c
// contoh: l3g4200d_adapter.h
#include "l3g4200d.h"
#include "some_accel_driver.h"
#include "imu_types.h"

static inline int my_sensor_to_imu_sample(imu_sample_t *out) {
    out->gx = l3g4200d_read_raw_x();
    out->ax = accel_read_raw_x();
    // ... isi semua 6 field ...
    return 0;
}
```

Classifier **tidak perlu diubah** sama sekali.

## Panduan Tuning Threshold

1. **Kumpulkan data:** rekam `imu_sample_t` per aktivitas via UART/log.
   Minimal 1 menit per aktivitas target.
2. **Jalankan hardware test** (`test_classifier_hw.c`) — single window
   mode mencetak semua fitur + threshold berdampingan.
3. **Analisis distribusi:** catat range fitur per kelas (lihat tabel di
   atas sebagai contoh).
4. **Update threshold:** edit `classifier_config.h`, recompile firmware.
5. **Opsional:** train `sklearn.tree.DecisionTreeClassifier(max_depth=4)`
   dari fitur integer, ekstrak threshold dari tree.

> **Penting:** Jika sensor range berubah (mis. ±2g → ±4g), skala LSB/g
> berubah dan **semua** threshold harus di-tuning ulang.

## Referensi

- Karantonis et al. (2006), "Implementation of a Real-Time Human Movement
  Classifier Using a Triaxial Accelerometer for Ambulatory Monitoring"
- Bourke et al. (2007), "Evaluation of a threshold-based tri-axial
  accelerometer fall detection algorithm"

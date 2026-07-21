# Activity Classifier — IMU Fusion (Gyro + Accel)

Library klasifikasi aktivitas real-time berbasis fusi data gyroscope +
accelerometer untuk platform PULPissimo (RISC-V, tanpa FPU, 64 kB unified
memory).

## Arsitektur Pipeline

```
 Sensor Driver          Adapter              Classifier Module
 ┌─────────────┐    ┌──────────────┐    ┌─────────────────────────────┐
 │  MPU-6050   │    │ mpu6050_to_  │    │                             │
 │  L3G4200D   │───>│ imu_sample() │───>│  imu_sample_t               │
 │  ...        │    │              │    │       │                     │
 └─────────────┘    └──────────────┘    │       ▼                     │
                                        │  Sliding Window (32 sample) │
                                        │       │                     │
                                        │       ▼                     │
                                        │  Feature Extraction         │
                                        │  ┌──────────┬──────────┐    │
                                        │  │  Accel   │  Gyro    │    │
                                        │  │  SMA     │  Energy  │    │
                                        │  │  SVM     │  ZCR     │    │
                                        │  │  Tilt    │  MeanAbs │    │
                                        │  └────┬─────┴────┬─────┘    │
                                        │       │          │          │
                                        │       ▼          ▼          │
                                        │  Decision Tree (fusi)       │
                                        │       │                     │
                                        │       ▼                     │
                                        │  activity_t (output)        │
                                        └─────────────────────────────┘
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

## Footprint Memori

| Komponen | Bytes | Catatan |
|----------|-------|---------|
| `window_buf[32]` | 384 | 32 × 12 byte (imu_sample_t) |
| `accel_features_t` | 12 | 3 × int32_t |
| `gyro_features_t` | 12 | 3 × int32_t |
| Control variables | 8 | window_idx + window_full |
| Stack (lokal) | ~64 | Akumulator di feature extraction |
| **Total RAM** | **~480** | **< 0.75% dari 64 kB** |

Code size (ROM/flash) diestimasi ~2–3 kB.

## Struktur File

```
Classification/
├── imu_types.h          Sensor abstraction (imu_sample_t, activity_t)
├── classifier_config.h  Semua threshold (tuning hub)
├── classifier.h         Public API
├── classifier.c         Implementasi (windowing + features + tree)
├── test/
│   ├── test_classifier.c   Unit test (host GCC, tanpa hardware)
│   └── Makefile
└── README.md

sensors/mpu6050/
└── mpu6050_adapter.h    Contoh adapter (BUKAN bagian classifier)
```

## Quick Start

### Kompilasi & Test (tanpa hardware)

```bash
cd libraries/Classification/test
make
```

### Integrasi dengan MPU-6050

```c
#include "mpu6050.h"
#include "mpu6050_adapter.h"
#include "../../Classification/classifier.h"

// Setelah mpu6050_init():
classifier_init();
while (1) {
    imu_sample_t sample;
    if (mpu6050_to_imu_sample(&sample) == MPU6050_OK) {
        activity_t act = classifier_update(&sample);
        if (act != ACTIVITY_UNKNOWN) {
            printf("Aktivitas: %d\n", act);
        }
    }
    // delay sesuai sampling rate
}
```

### Integrasi dengan Sensor Lain

Buat adapter baru yang mengisi `imu_sample_t`:

```c
// contoh: l3g4200d_adapter.h
#include "l3g4200d.h"
#include "some_accel_driver.h"
#include "../../Classification/imu_types.h"

static inline int my_sensor_to_imu_sample(imu_sample_t *out) {
    out->gx = l3g4200d_read_raw_x();  // dll
    out->ax = accel_read_raw_x();      // dll
    return 0;
}
```

Classifier **tidak perlu diubah** sama sekali.

## Panduan Tuning Threshold

1. **Kumpulkan data**: rekam `imu_sample_t` per aktivitas via UART
2. **Ekstrak fitur offline**: jalankan `extract_features_accel/gyro()` pada data
3. **Analisis distribusi**: plot fitur per kelas, cari batas pemisah
4. **Opsional**: train `sklearn.tree.DecisionTreeClassifier(max_depth=4)`,
   ekstrak threshold dari tree
5. **Update `classifier_config.h`**: masukkan threshold baru, recompile

## Referensi

- Karantonis et al. (2006), "Implementation of a Real-Time Human Movement
  Classifier Using a Triaxial Accelerometer for Ambulatory Monitoring"
- Bourke et al. (2007), "Evaluation of a threshold-based tri-axial
  accelerometer fall detection algorithm"

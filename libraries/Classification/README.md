# Activity Classifier

Classifier aktivitas berbasis data IMU yang menggunakan accelerometer dan gyroscope untuk mengenali beberapa aktivitas dasar secara real-time.

Classifier menggunakan pendekatan **hierarchical decision tree** dengan **temporal majority vote** untuk mengurangi perubahan output yang terlalu cepat antar-window.

Kelas aktivitas yang tersedia:

* `SIT`
* `STAND`
* `LIE`
* `WALK`
* `FALL`

`UNKNOWN` digunakan ketika window belum memiliki cukup sampel untuk dilakukan klasifikasi.

## 1. Arsitektur

Secara umum, alur classifier adalah:

```text
IMU Sample
    |
    v
Sliding Window
    |
    v
Feature Extraction
    |
    +--------------------+
    |                    |
    v                    v
Accelerometer         Gyroscope
features              features
    |                    |
    +---------+----------+
              |
              v
      Hierarchical Decision Tree
              |
              v
       Raw Classification
              |
              v
        Majority Vote
              |
              v
        Final Activity
```

Classifier memproses data dalam window berukuran:

```c
CLF_WINDOW_SIZE = 32
```

Dengan sampling rate 50 Hz, satu window mencakup sekitar:

```text
32 / 50 = 640 ms
```

Hasil klasifikasi kemudian disimpan dalam history sebanyak:

```c
CLF_HISTORY_SIZE = 3
```

dan digunakan untuk majority vote.

---

## 2. Public API

### `classifier_init()`

Menginisialisasi seluruh state classifier.

```c
void classifier_init(void);
```

Wajib dipanggil sebelum classifier digunakan.

Contoh:

```c
classifier_init();
```

---

### `classifier_push_sample()`

Memasukkan satu sampel IMU ke dalam sliding window.

```c
int classifier_push_sample(const imu_sample_t *sample);
```

Return:

* `0`: window belum penuh
* `1`: window sudah penuh dan siap diklasifikasikan

Contoh:

```c
imu_sample_t sample;

sample.ax = ax;
sample.ay = ay;
sample.az = az;
sample.gx = gx;
sample.gy = gy;
sample.gz = gz;

if (classifier_push_sample(&sample)) {
    activity_t activity = classifier_classify();
}
```

---

### `classifier_classify()`

Melakukan klasifikasi terhadap window yang sudah penuh.

```c
activity_t classifier_classify(void);
```

Jika window belum penuh, fungsi mengembalikan:

```c
ACTIVITY_UNKNOWN
```

Setelah klasifikasi dilakukan, window akan dikosongkan dan siap menerima window berikutnya.

---

### `classifier_update()`

Convenience API untuk melakukan push dan classification sekaligus.

```c
activity_t classifier_update(const imu_sample_t *sample);
```

Contoh penggunaan:

```c
activity_t activity = classifier_update(&sample);

if (activity != ACTIVITY_UNKNOWN) {
    // gunakan hasil klasifikasi
}
```

Fungsi ini akan mengembalikan `ACTIVITY_UNKNOWN` selama window belum penuh.

---

## 3. Feature Extraction

Feature extraction dapat diakses secara langsung untuk kebutuhan debugging, tuning threshold, dan eksperimen.

### Accelerometer

```c
void extract_features_accel(
    const imu_sample_t *buf,
    int n,
    accel_features_t *feat
);
```

Menghasilkan:

### SMA

```text
Σ(|ax - mean_ax| +
  |ay - mean_ay| +
  |az - mean_az|)
```

SMA menggunakan deviasi terhadap rata-rata acceleration dalam satu window.

Tujuannya adalah mengurangi pengaruh orientasi statis sensor sehingga fitur lebih merepresentasikan intensitas gerakan.

### SVM peak

```text
max(ax² + ay² + az²)
```

SVM dihitung dari acceleration raw tanpa `sqrt()`.

Fitur ini digunakan untuk mendeteksi magnitude benturan pada fall detection.

### Tilt ratio

```text
(sum_az × 1024) /
(|sum_ax| + |sum_ay| + |sum_az| + N)
```

Digunakan untuk memperkirakan orientasi tubuh dan membedakan:

```text
STAND
SIT
LIE
```

---

### Gyroscope

```c
void extract_features_gyro(
    const imu_sample_t *buf,
    int n,
    gyro_features_t *feat
);
```

Menghasilkan:

* `energy`
* `zcr`
* `mean_abs`

Gyroscope energy dihitung menggunakan right shift 4 bit sebelum kuadrat untuk mengurangi risiko overflow.

ZCR menghitung perubahan tanda pada sumbu `gx`.

---

## 4. Decision Tree

Classifier menggunakan beberapa tingkat keputusan.

```text
                 SMA
                  |
        +---------+---------+
        |                   |
      REST                ACTIVE
        |                   |
      TILT                FALL?
        |                   |
   +----+----+          +---+---+
   |    |    |          |       |
 STAND SIT  LIE        FALL    WALK?
                                  |
                              +---+---+
                              |       |
                            WALK    POSTURE
```

### Level 1: Rest vs Active

SMA dibandingkan dengan:

```c
CLF_SMA_REST_THRESHOLD
```

Threshold ini menentukan apakah intensitas gerakan dalam window cukup rendah untuk diperlakukan sebagai kondisi rest.

### Level 2: Posture

Postur ditentukan berdasarkan `tilt_ratio`.

```text
tilt tinggi       -> STAND
tilt menengah     -> SIT
tilt mendekati 0  -> LIE
```

Posture classification digunakan pada kondisi rest maupun sebagai fallback ketika window aktif tidak memenuhi pola fall atau walk.

### Level 3: Fall

Fall membutuhkan dua kondisi sekaligus:

```text
SVM acceleration peak
+
Gyroscope energy
```

Tujuannya mengurangi kemungkinan benturan acceleration saja dianggap sebagai jatuh.

Fall tidak melewati majority vote dan langsung dikeluarkan sebagai `ACTIVITY_FALL`.

### Level 4: Walk

Walking ditentukan berdasarkan:

```text
Gyroscope ZCR
+
Gyroscope energy
```

Kelas RUN belum dibedakan secara khusus. Gerakan aktif periodik yang memenuhi threshold walking akan dikategorikan sebagai `WALK`.

---

## 5. Temporal Majority Vote

Hasil setiap window disimpan dalam ring buffer:

```c
CLF_HISTORY_SIZE = 3
```

Classifier kemudian menghitung kelas yang paling sering muncul.

Tujuannya adalah mengurangi output yang berubah-ubah akibat variasi kecil antar-window.

Contoh:

```text
Window 1 -> SIT
Window 2 -> STAND
Window 3 -> SIT
```

Hasil majority vote:

```text
SIT
```

Fall merupakan pengecualian. Ketika fall terdeteksi, hasil langsung dikembalikan tanpa majority vote.

---

## 6. Threshold dan Tuning

Threshold saat ini merupakan parameter yang dapat diubah di `classifier.h`.

Parameter utama:

```c
CLF_SMA_REST_THRESHOLD
CLF_TILT_STAND_THRESHOLD
CLF_TILT_LIE_THRESHOLD
CLF_SVM_FALL_THRESHOLD
CLF_GYRO_ENERGY_FALL_THRESHOLD
CLF_GYRO_ENERGY_WALK_THRESHOLD
CLF_GYRO_ZCR_WALK_MIN
```

Threshold sebaiknya tidak dianggap sebagai nilai universal. Nilainya dipengaruhi oleh:

* sampling rate
* skala raw sensor
* posisi pemasangan sensor
* orientasi sensor
* karakteristik noise
* ukuran window
* intensitas gerakan pengguna

Karena itu, threshold perlu divalidasi menggunakan data lapangan sebelum classifier digunakan sebagai sistem final.

---

## Referensi

Implementasi ini menggunakan pendekatan feature-based dan hierarchical classification yang memiliki dasar pada penelitian mengenai human movement classification menggunakan triaxial accelerometer.

**Referensi utama:**

> Karantonis, D. M., Narayanan, M. R., Mathie, M., Lovell, N. H., & Celler, B. G. (2006). *Implementation of a Real-Time Human Movement Classifier Using a Triaxial Accelerometer for Ambulatory Monitoring.*

**Link jurnal:**

```text
https://www.researchgate.net/publication/3415807_Implementation_of_a_Real-Time_Human_Movement_Classifier_Using_a_Triaxial_Accelerometer_for_Ambulatory_Monitoring
```

Referensi digunakan sebagai dasar pengembangan dan pembanding metode. Implementasi pada library ini merupakan adaptasi yang disederhanakan untuk kebutuhan embedded system dan belum identik dengan metode penelitian asli.

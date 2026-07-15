# L3G4200D Gyroscope Sensor Library

Driver library minimal untuk STMicroelectronics **L3G4200D** 3-Axis Digital Gyroscope pada board ICDeC FPGA (PULPissimo + Intel Cyclone 10), dibangun di atas [pulp-runtime](https://github.com/iabdurrahman/pulp-runtime.git) I2C driver.

Hanya berisi register yang benar-benar dipakai. Bukan port lengkap dari seluruh peta register L3G4200D.

## Fitur

- API minimal dan langsung pakai — hanya fungsi yang sudah terverifikasi di hardware
- Baca data gyroscope mentah (16-bit signed) dan dalam °/s (fixed-point integer)
- Konfigurasi full-scale range: ±250, ±500, ±2000 °/s
- Auto-detection alamat I2C (mencoba default, lalu alternatif)
- Error handling dengan status code yang jelas
- **Tanpa floating-point** — semua konversi pakai integer fixed-point (×100)

## Catatan Penting (Temuan dari Debugging)

> [!IMPORTANT]
> Pin SDO set HIGH untuk alamat I2C 0x69
> Pin CS set HIGH untuk menggunakan komunikasi I2C

### 1. PULPissimo Tidak Punya FPU Hardware

Semua konversi ke deg/s dilakukan dengan **integer fixed-point** (dps × 100), **bukan float**. Float terbukti menghasilkan nilai yang salah di PULPissimo.

Contoh: nilai `x_x100 = 875` artinya **8.75 °/s**.

### 2. Delay Wajib antara Write dan Read (Repeated-Start)

Perlu delay kecil (~200 loop iterasi) antara fase "tulis alamat register" dan fase "baca data" (repeated-start). Tanpa ini, hasil baca **selalu 0xFF** meski tidak ada error/timeout dari `i2c_write()`/`i2c_read()`. Delay ini sudah dibangun ke dalam fungsi internal `l3g_read_reg()`.

### 3. Output Data Little-Endian

L3G4200D mengeluarkan data dalam format **little-endian** — byte Low duluan, baru byte High. Ini berbeda dari MPU-6050 yang big-endian.

```
Register 0x28 = OUT_X_L (low byte)
Register 0x29 = OUT_X_H (high byte)
→ X = (OUT_X_H << 8) | OUT_X_L
```

### 4. Multi-Byte Read: Bit Auto-Increment Wajib

Untuk membaca lebih dari 1 byte berurutan, bit MSB (0x80) pada alamat register awal **wajib di-set** supaya auto-increment aktif.

```c
// Contoh: baca 6 byte dari OUT_X_L (0x28)
uint8_t reg = 0x28 | 0x80;  // = 0xA8
i2c_write(i2c, &reg, 1, 0); // tanpa stop → repeated start
i2c_read(i2c, buf, 6, 0);   // baca 6 byte: XL, XH, YL, YH, ZL, ZH
```

## Struktur File

```
l3g4200d/
├── l3g4200d.h            # Public API header
├── l3g4200d.c            # Implementasi driver
└── README.md             # Dokumentasi ini
└── test/
    ├── test_l3g4200d.c       # Hardware test untuk FPGA
    └── Makefile              # Build system untuk hardware test
```

## Cara Build & Run

### Hardware Test (FPGA)

Build dan jalankan di board ICDeC:

```bash
# 1. Setup environment

export PATH="$PATH:/home/tetandclee/ICDeC/chroot/bin"
source recurse-submodules/configs/pulpissimo.sh

# 2. Build
cd libraries/sensors/l3g4200d/test
make all

# 3. Run di FPGA
make run platform=fpga

# 4. Clean
rm -rf build
```

## Quick Start

```c
#include "l3g4200d.h"

int main() {
    l3g4200d_config_t cfg;
    l3g4200d_dps_x100_t dps;

    l3g4200d_default_config(&cfg);       // Load default config
    l3g4200d_init(&cfg);                 // Init sensor + verify WHO_AM_I
    l3g4200d_read_dps_x100(&dps);        // Baca data dalam °/s (×100)

    // Contoh: dps.x_x100 = 875 artinya 8.75 °/s
    printf("X=%d.%02d Y=%d.%02d Z=%d.%02d dps\n",
           dps.x_x100 / 100, abs(dps.x_x100 % 100),
           dps.y_x100 / 100, abs(dps.y_x100 % 100),
           dps.z_x100 / 100, abs(dps.z_x100 % 100));

    l3g4200d_deinit();                   // Power down + close I2C
}
```

---

## API Documentation

### Status Codes

```c
typedef enum {
    L3G4200D_OK          =  0,  // Operasi berhasil
    L3G4200D_ERR_I2C     = -1,  // I2C gagal buka / komunikasi umum
    L3G4200D_ERR_ID      = -2,  // WHO_AM_I tidak sesuai (bukan L3G4200D)
    L3G4200D_ERR_CONFIG  = -3,  // Gagal konfigurasi register, atau fungsi dipanggil sebelum init
    L3G4200D_ERR_TIMEOUT = -4,  // Transaksi I2C timeout
    L3G4200D_ERR_NULL    = -5   // Pointer argumen NULL
} l3g4200d_status_t;
```

### Data Structures

#### `l3g4200d_raw_t` — Data Mentah Gyroscope

```c
typedef struct {
    int16_t x;  // Raw angular rate data, sumbu X
    int16_t y;  // Raw angular rate data, sumbu Y
    int16_t z;  // Raw angular rate data, sumbu Z
} l3g4200d_raw_t;
```

Berisi nilai 16-bit signed langsung dari register sensor, tanpa konversi. Untuk mendapatkan °/s, gunakan `l3g4200d_read_dps_x100()` atau kalikan manual dengan sensitivity factor.

#### `l3g4200d_dps_x100_t` — Data Gyroscope dalam °/s (Fixed-Point ×100)

```c
typedef struct {
    int32_t x_x100;  // Angular rate sumbu X, dikali 100
    int32_t y_x100;  // Angular rate sumbu Y, dikali 100
    int32_t z_x100;  // Angular rate sumbu Z, dikali 100
} l3g4200d_dps_x100_t;
```

Format **fixed-point integer** dipakai karena PULPissimo tidak punya FPU. Nilai sudah dikalikan 100 sehingga presisi desimal tetap tersimpan dalam integer.

| Contoh `x_x100` | Artinya |
|--|--|
| `875` | 8.75 °/s |
| `-1750` | -17.50 °/s |
| `7000` | 70.00 °/s |

#### `l3g4200d_range_t` — Full-Scale Range

```c
typedef enum {
    L3G4200D_RANGE_250DPS  = 0x00,  // ±250 °/s  (sensitivitas: 8.75 mdps/digit)
    L3G4200D_RANGE_500DPS  = 0x10,  // ±500 °/s  (sensitivitas: 17.50 mdps/digit)
    L3G4200D_RANGE_2000DPS = 0x20   // ±2000 °/s (sensitivitas: 70.00 mdps/digit)
} l3g4200d_range_t;
```

Nilai-nilai ini langsung merupakan bit-field untuk register CTRL_REG4 (bits FS1:FS0). Range yang lebih tinggi = sensitivitas lebih rendah tetapi rentang pengukuran lebih lebar.

#### `l3g4200d_config_t` — Konfigurasi Inisialisasi

```c
typedef struct {
    uint8_t          i2c_addr;   // 7-bit I2C address (0x69 atau 0x68)
    int              i2c_id;     // I2C peripheral ID (biasanya 0)
    unsigned int     i2c_freq;   // Baudrate I2C (disarankan 100000 / 100kHz)
    l3g4200d_range_t range;      // Full-scale range awal
} l3g4200d_config_t;
```

Set field-field ini sebelum memanggil `l3g4200d_init()`. Gunakan `l3g4200d_default_config()` untuk mendapatkan konfigurasi default yang sudah terbukti bekerja.

### API Functions

---

#### `l3g4200d_default_config`

```c
l3g4200d_status_t l3g4200d_default_config(l3g4200d_config_t *cfg);
```

Mengisi struct konfigurasi dengan nilai default yang sudah **terbukti bekerja** di hardware.

**Nilai default:**

| Field | Nilai | Keterangan |
|-------|-------|------------|
| `i2c_addr` | `0x69` | SDO pin = HIGH (Vdd) |
| `i2c_id` | `0` | I2C peripheral pertama |
| `i2c_freq` | `100000` | 100 kHz (I2C standard mode) |
| `range` | `L3G4200D_RANGE_250DPS` | ±250 °/s |

**Parameter:**
- `[out] cfg` — Pointer ke struct konfigurasi yang akan diisi.

**Return:**
- `L3G4200D_OK` — Berhasil.
- `L3G4200D_ERR_NULL` — `cfg` adalah NULL.

---

#### `l3g4200d_init`

```c
l3g4200d_status_t l3g4200d_init(l3g4200d_config_t *cfg);
```

Inisialisasi sensor L3G4200D. Langkah-langkah yang dilakukan:

1. Membuka bus I2C dengan alamat dari `cfg->i2c_addr`
2. Membaca register **WHO_AM_I** (0x0F) — harus bernilai `0xD3`
3. Jika gagal di alamat default, otomatis mencoba alamat alternatif (0x69 ↔ 0x68) — **auto-detection**
4. Menulis **CTRL_REG1** (0x20) = `0x0F` — power up + enable XYZ + ODR 100Hz
5. Readback & verifikasi CTRL_REG1
6. Menulis **CTRL_REG4** (0x23) = nilai range dari config
7. Readback & verifikasi CTRL_REG4

> [!NOTE]
> Setelah init berhasil, field `cfg->i2c_addr` akan diupdate jika auto-detection mengubah alamat.
> Setiap konfigurasi register diverifikasi dengan readback — jika tidak cocok, return `L3G4200D_ERR_CONFIG`.

**Parameter:**
- `[in,out] cfg` — Pointer ke struct konfigurasi. Diupdate jika alamat berubah via auto-detection.

**Return:**
- `L3G4200D_OK` — Inisialisasi berhasil.
- `L3G4200D_ERR_NULL` — `cfg` adalah NULL.
- `L3G4200D_ERR_I2C` — Gagal membuka bus I2C.
- `L3G4200D_ERR_TIMEOUT` — Tidak ada respons dari sensor (I2C timeout).
- `L3G4200D_ERR_ID` — WHO_AM_I tidak cocok (bukan L3G4200D, atau sensor salah).
- `L3G4200D_ERR_CONFIG` — Readback verifikasi register gagal.

---

#### `l3g4200d_who_am_i`

```c
l3g4200d_status_t l3g4200d_who_am_i(uint8_t *id);
```

Membaca register **WHO_AM_I** (0x0F). Berguna untuk verifikasi manual apakah sensor masih terhubung dan responsif. Nilai yang diharapkan: **`0xD3`**.

> [!NOTE]
> Harus dipanggil **setelah** `l3g4200d_init()`. Jika dipanggil sebelum init, return `L3G4200D_ERR_CONFIG`.

**Parameter:**
- `[out] id` — Pointer untuk menyimpan nilai WHO_AM_I.

**Return:**
- `L3G4200D_OK` — Berhasil.
- `L3G4200D_ERR_NULL` — `id` adalah NULL.
- `L3G4200D_ERR_CONFIG` — Sensor belum diinisialisasi.
- `L3G4200D_ERR_TIMEOUT` — Komunikasi I2C timeout.

---

#### `l3g4200d_read_raw`

```c
l3g4200d_status_t l3g4200d_read_raw(l3g4200d_raw_t *raw);
```

Membaca data gyroscope mentah dari ketiga sumbu sekaligus. Mengembalikan nilai 16-bit signed langsung dari register sensor, tanpa scaling apapun.

Secara internal, membaca 6 byte berturut-turut dari register OUT_X_L (0x28) menggunakan auto-increment (bit 0x80). Data sensor di-reassemble dari little-endian:

```
raw->x = (OUT_X_H << 8) | OUT_X_L
raw->y = (OUT_Y_H << 8) | OUT_Y_L
raw->z = (OUT_Z_H << 8) | OUT_Z_L
```

**Parameter:**
- `[out] raw` — Pointer ke `l3g4200d_raw_t` untuk menyimpan data.

**Return:**
- `L3G4200D_OK` — Berhasil.
- `L3G4200D_ERR_NULL` — `raw` adalah NULL.
- `L3G4200D_ERR_CONFIG` — Sensor belum diinisialisasi.
- `L3G4200D_ERR_TIMEOUT` — Komunikasi I2C timeout / byte tidak lengkap.

---

#### `l3g4200d_read_dps_x100`

```c
l3g4200d_status_t l3g4200d_read_dps_x100(l3g4200d_dps_x100_t *dps);
```

Membaca data gyroscope dan mengkonversi ke **derajat per detik** (°/s) dalam format fixed-point integer (dikali 100).

Secara internal: membaca raw data, lalu menghitung:

```
dps_x100 = (raw_value × sensitivity_num) / sensitivity_denom
```

Dimana sensitivity berdasarkan range aktif:

| Range | sensitivity_num | sensitivity_denom | Sensitivitas |
|-------|----------------|-------------------|-------------|
| ±250 °/s | 875 | 1000 | 8.75 mdps/digit |
| ±500 °/s | 1750 | 1000 | 17.50 mdps/digit |
| ±2000 °/s | 7000 | 1000 | 70.00 mdps/digit |

> [!TIP]
> Untuk menampilkan dengan desimal: `printf("%d.%02d", val / 100, abs(val % 100));`

**Parameter:**
- `[out] dps` — Pointer ke `l3g4200d_dps_x100_t` untuk menyimpan data.

**Return:**
- `L3G4200D_OK` — Berhasil.
- `L3G4200D_ERR_NULL` — `dps` adalah NULL.
- `L3G4200D_ERR_CONFIG` — Sensor belum diinisialisasi.
- `L3G4200D_ERR_TIMEOUT` — Komunikasi I2C timeout.

---

#### `l3g4200d_set_range`

```c
l3g4200d_status_t l3g4200d_set_range(l3g4200d_range_t range);
```

Mengubah full-scale range saat runtime. Bisa dipanggil setelah inisialisasi untuk mengubah range secara dinamis. Menulis nilai baru ke CTRL_REG4 (0x23) dan mengupdate sensitivity internal.

**Parameter:**
- `[in] range` — Pilihan full-scale range baru (`L3G4200D_RANGE_250DPS`, `L3G4200D_RANGE_500DPS`, atau `L3G4200D_RANGE_2000DPS`).

**Return:**
- `L3G4200D_OK` — Berhasil.
- `L3G4200D_ERR_CONFIG` — Sensor belum diinisialisasi.
- `L3G4200D_ERR_I2C` — Gagal menulis ke register.

---

#### `l3g4200d_deinit`

```c
l3g4200d_status_t l3g4200d_deinit(void);
```

De-inisialisasi sensor L3G4200D:

1. Menulis CTRL_REG1 = `0x00` (power-down mode)
2. Menutup bus I2C
3. Mereset internal driver state

Aman dipanggil meskipun sensor belum diinisialisasi (langsung return `L3G4200D_OK`).

**Return:**
- `L3G4200D_OK` — Selalu berhasil.

---

## Register Map

Hanya register yang dipakai oleh library ini:

| Register | Alamat | Nama | Deskripsi |
|----------|--------|------|-----------|
| WHO_AM_I | `0x0F` | `L3G4200D_REG_WHO_AM_I` | Device ID, read-only, default `0xD3` |
| CTRL_REG1 | `0x20` | `L3G4200D_REG_CTRL_REG1` | ODR, bandwidth, power mode, axis enable |
| CTRL_REG4 | `0x23` | `L3G4200D_REG_CTRL_REG4` | Full-scale range selection |
| OUT_X_L | `0x28` | `L3G4200D_REG_OUT_X_L` | Awal data gyro (6 byte berurutan: XL,XH,YL,YH,ZL,ZH) |

### Konstanta Penting

| Macro | Nilai | Keterangan |
|-------|-------|------------|
| `L3G4200D_AUTO_INCREMENT_BIT` | `0x80` | OR-kan ke alamat register saat baca >1 byte |
| `L3G4200D_WHO_AM_I_VALUE` | `0xD3` | Nilai WHO_AM_I yang benar |
| `L3G4200D_CTRL_REG1_NORMAL_XYZ_100HZ` | `0x0F` | PD=1, X/Y/Z enable, ODR=100Hz |
| `L3G4200D_CTRL_REG1_POWER_DOWN` | `0x00` | Power-down mode |

### Konfigurasi Internal (Tidak Bisa Diubah User)

| Macro | Nilai | Keterangan |
|-------|-------|------------|
| `L3G4200D_I2C_TIMEOUT_US` | `5000` | Software timeout per transaksi I2C (µs) |
| `L3G4200D_READ_DELAY_LOOPS` | `200` | Delay antara write-register dan read-data |
| `L3G4200D_CONFIG_SETTLE_LOOPS` | `50000` | Delay setelah menulis register konfigurasi |

## Alamat I2C

| Macro | Alamat | Kondisi |
|-------|--------|---------|
| `L3G4200D_I2C_ADDR_DEFAULT` | `0x69` | SDO pin = HIGH (Vdd) |
| `L3G4200D_I2C_ADDR_ALT` | `0x68` | SDO pin = LOW (GND) |

Saat inisialisasi, driver akan mencoba alamat default terlebih dahulu. Jika gagal (WHO_AM_I tidak cocok atau I2C timeout), otomatis mencoba alamat alternatif.

## Unit Test

Library ini dilengkapi 21 unit test dengan Mock I2C yang memverifikasi:

| Test | Deskripsi |
|------|-----------|
| 01 | Default config values (addr, freq, range) |
| 02 | `default_config(NULL)` → `ERR_NULL` |
| 03 | I2C address: 7-bit ke 8-bit conversion (0x69 → 0xD2) |
| 04 | WHO_AM_I verification (0xD3 = pass, lainnya = fail) |
| 05 | CTRL_REG1 = 0x0F (power on + XYZ enable) |
| 06 | CTRL_REG4 per range (BDU + full-scale) |
| 07 | Raw data assembly (little-endian, signed 16-bit) |
| 08 | Auto-increment bit (0x80) pada multi-byte read |
| 09 | Single byte read tanpa auto-increment |
| 10-12 | Sensitivity per range (250/500/2000 dps) |
| 13 | `set_range()` mengubah CTRL_REG4 |
| 14 | `set_range()` mengupdate sensitivity |
| 15 | `deinit()` power-down (CTRL_REG1 = 0x00) |
| 16 | Read sebelum init → `ERR_CONFIG` |
| 17 | NULL pointer handling |
| 18 | I2C failure handling |
| 19 | Negative raw values (two's complement) |
| 20 | Timeout retry recovery |
| 21 | Timeout exhausted → `ERR_TIMEOUT` |

```
L3G4200D: 21 tests, all passed ✓
```

## Troubleshooting

| Masalah | Solusi |
|---------|--------|
| `L3G4200D_ERR_I2C` | Cek koneksi SDA/SCL, pastikan pull-up resistor terpasang |
| `L3G4200D_ERR_ID` | Cek modul sensor & alamat I2C (SDO pin HIGH/LOW) |
| `L3G4200D_ERR_TIMEOUT` | Reset board, cek pull-up resistor, cek apakah sensor terpasang dengan benar |
| `L3G4200D_ERR_CONFIG` | Pastikan `l3g4200d_init()` sudah dipanggil sebelum read/write |
| Data selalu 0xFF | Delay antara write dan read mungkin terlalu kecil — cek `L3G4200D_READ_DELAY_LOOPS` |
| Data selalu 0 | Tambah delay setelah init (~100ms) sebelum membaca data pertama |
| Build error | Jalankan `source configs/pulpissimo.sh` terlebih dahulu |

## Referensi

- **Datasheet**: [L3G4200D (ST Microelectronics)](https://www.st.com/resource/en/datasheet/l3g4200d.pdf)
- **Platform**: [pulp-runtime I2C driver](https://github.com/iabdurrahman/pulp-runtime.git)

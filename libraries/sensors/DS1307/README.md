# Bare-Metal RTC DS1307 Driver for RISC-V (PULPissimo)

Repositori ini berisi implementasi *driver bare-metal* bahasa C untuk membaca dan mengatur waktu pada sensor Real-Time Clock (RTC) DS1307. Driver ini dirancang khusus untuk berjalan di atas prosesor RISC-V NUSACORE pada *board* ARDUNESIA FPGA C10.

## Struktur File
- `rtc.h`: Deklarasi struktur data API, error codes, dan *header* fungsi.
- `rtc.c`: Logika utama *driver*, implementasi I2C Read/Write, konversi BCD, dan *bit-masking*.
- `test.c`: Kode program utama (*entry point*) yang akan di-*flash* ke *board* FPGA untuk mencetak waktu via UART. Dilengkapi error handling detail dan validasi per-register.
- `test_host.c`: Unit test lokal berbasis memori untuk mensimulasikan pembacaan I2C dan memvalidasi perhitungan matematika BCD tanpa perlu menyambungkan *hardware*.
- `build.sh` & `makefile`: *Script* otomatisasi *environment* dan kompilasi menggunakan PULP Runtime.

---

## Penjelasan Logika 

### 1. Address Handling (Chip-Select)
Alamat I2C DS1307 dari datasheet adalah `0x68` (7-bit, binary: `1101000`). Driver I2C PULP Runtime membutuhkan format 8-bit, sehingga address di-shift kiri 1 bit saat inisialisasi:
- **Chip-select (cs):** `0x68 << 1` = `0xD0`
- **Write address (otomatis):** `0xD0` (bit-0 = 0)
- **Read address (otomatis):** `0xD0 | 0x1` = `0xD1` (bit-0 = 1)

Address di-set melalui `dev->cs = DS1307_ADDR << 1` saat inisialisasi. Library `i2c.c` secara otomatis mengirim `dev->cs` sebagai address byte setelah START condition, dan melakukan OR dengan `0x1` saat operasi read. **Address tidak boleh dimasukkan ke dalam buffer data.**

### 2. Konversi BCD (Binary-Coded Decimal)
Sensor DS1307 hanya menerima dan mengirim data dalam format BCD. Agar API lebih ramah digunakan (*user-friendly*), struktur `rtc_time_t` sengaja dirancang menggunakan nilai **Desimal murni (0-99)**. Driver ini secara otomatis melakukan konversi `dec_to_bcd` saat menulis ke sensor (*set time*), dan `bcd_to_dec` saat membaca dari sensor (*get time*).

### 3. Burst Read I2C (Auto-Increment Pointer)
Untuk efisiensi jalur komunikasi I2C, *driver* tidak memanggil alamat register satu per satu. Pembacaan dilakukan dengan teknik **Burst Read**:
- Menembak titik awal register `0x00` (Seconds) tanpa STOP condition (repeated start).
- Menyedot 7 Byte data secara berurutan dalam satu kali tarikan (dari *Seconds* hingga *Year*) memanfaatkan fitur *Internal Auto-Increment Pointer* milik DS1307.

### 4. Bit-Masking & Konfigurasi Hardware
Data mentah di dalam register DS1307 bercampur dengan bit konfigurasi. Driver ini menggunakan operasi bitwise untuk membersihkan data tersebut:
- **`& 0x7F` pada Seconds:** Digunakan untuk membuang bit ke-7 (Clock Halt / CH) agar waktu murni terbaca, sekaligus memastikan bit CH selalu bernilai `0` saat *set time* agar osilator jam tetap menyala.
- **`& 0x3F` pada Hours:** Digunakan untuk mengabaikan bit ke-6 (Mode 12/24), sehingga pembacaan secara konstan terkunci pada format **24-Jam**.

### 5. Error Handling
Driver menggunakan **error code granular** untuk setiap titik kegagalan, sehingga debugging di board dapat langsung menunjukkan operasi mana yang gagal. Lihat tabel error codes di bawah.

---

## Dokumentasi API

### Struktur Data
```c
typedef struct {
    uint8_t seconds; // Detik (0-59)
    uint8_t minutes; // Menit (0-59)
    uint8_t hours;   // Jam (0-23) - mode 24 jam
    uint8_t day;     // Hari (1-7, 1=Minggu)
    uint8_t date;    // Tanggal (1-31)
    uint8_t month;   // Bulan (1-12)
    uint8_t year;    // Tahun (0-99, representasi 2000-2099)
} rtc_time_t;
```
Struktur Pengecek Pergantian Hari
```c
typedef struct {
    uint8_t last_date;
    uint8_t last_month;
    uint8_t last_year;
    int     valid;
} day_tracker_t;
```

### Error Codes

| Code | Macro | Keterangan |
|------|-------|------------|
| `0` | `RTC_OK` | Sukses, tidak ada error |
| `-1` | `RTC_ERR_NULL_PTR` | Parameter NULL (pointer tidak valid) |
| `-2` | `RTC_ERR_I2C_OPEN` | Gagal membuka peripheral I2C (`i2c_open`) |
| `-3` | `RTC_ERR_VALIDATION` | Nilai waktu diluar range (cek datasheet DS1307) |
| `-4` | `RTC_ERR_I2C_WRITE_SET` | Gagal `i2c_write` saat SET time (kirim 8 byte) |
| `-5` | `RTC_ERR_I2C_WRITE_PTR` | Gagal `i2c_write` saat set register pointer (tahap 1 GET) |
| `-6` | `RTC_ERR_I2C_WRITE_READ` | Gagal `i2c_write` address READ (tahap 2 GET) |
| `-7` | `RTC_ERR_I2C_READ` | Gagal `i2c_read` burst 7 byte (tahap 2 GET) |

### Fungsi

#### `int rtc_init(i2c_dev_t *dev_conf, i2c_t **i2c_handle)`
Menginisialisasi hardware I2C Bus 0 untuk komunikasi dengan DS1307. Mengatur chip-select ke address `0x68 << 1` (7-bit di-shift ke 8-bit) dan baudrate ke 100 kHz.

| Parameter | Deskripsi |
|---|---|
| `dev_conf` | Pointer ke struktur konfigurasi I2C (dialokasi pemanggil) |
| `i2c_handle` | Pointer-to-pointer yang akan diisi handle I2C hasil `i2c_open()` |

| Return | Keterangan |
|---|---|
| `RTC_OK` (0) | Inisialisasi berhasil |
| `RTC_ERR_NULL_PTR` (-1) | Parameter `NULL` |
| `RTC_ERR_I2C_OPEN` (-2) | `i2c_open()` gagal |

**Contoh:**
```c
i2c_dev_t rtc_dev_conf;
i2c_t *rtc_handle;

int rc = rtc_init(&rtc_dev_conf, &rtc_handle);
if (rc != RTC_OK) {
    printf("Init gagal! code: %d\n", rc);
}
```

---

#### `int rtc_set_time(i2c_t *i2c_handle, rtc_time_t *time)`
Mengatur waktu pada DS1307 sekaligus mengaktifkan osilator (clear bit CH).

| Parameter | Deskripsi |
|---|---|
| `i2c_handle` | Handle I2C yang diperoleh dari `rtc_init()` |
| `time` | Pointer ke struktur `rtc_time_t` berisi waktu dalam format desimal |

| Return | Keterangan |
|---|---|
| `RTC_OK` (0) | Waktu berhasil diset |
| `RTC_ERR_NULL_PTR` (-1) | Parameter `NULL` |
| `RTC_ERR_VALIDATION` (-3) | Nilai waktu diluar range |
| `RTC_ERR_I2C_WRITE_SET` (-4) | I2C write gagal |

**Validasi range otomatis:** seconds 0-59, minutes 0-59, hours 0-23, day 1-7, date 1-31, month 1-12, year 0-99.

**Contoh:**
```c
rtc_time_t waktu = {
    .seconds = 0,
    .minutes = 30,
    .hours   = 14,
    .day     = 4,      // Kamis
    .date    = 26,
    .month   = 6,
    .year    = 26       // 2026
};

int rc = rtc_set_time(rtc_handle, &waktu);
if (rc != RTC_OK) {
    printf("Set time gagal! code: %d\n", rc);
}
```

---

#### `int rtc_get_time(i2c_t *i2c_handle, rtc_time_t *time)`
Membaca waktu terkini dari DS1307 menggunakan teknik *burst read* (7 byte sekaligus).

| Parameter | Deskripsi |
|---|---|
| `i2c_handle` | Handle I2C yang diperoleh dari `rtc_init()` |
| `time` | Pointer ke struktur `rtc_time_t` yang akan diisi hasil bacaan (dalam format desimal) |

| Return | Keterangan |
|---|---|
| `RTC_OK` (0) | Waktu berhasil dibaca |
| `RTC_ERR_NULL_PTR` (-1) | Parameter `NULL` |
| `RTC_ERR_I2C_WRITE_PTR` (-5) | Gagal set register pointer |
| `RTC_ERR_I2C_READ` (-7) | Gagal burst read 7 byte |

**Contoh:**
```c
rtc_time_t waktu_baca = {0};

int rc = rtc_get_time(rtc_handle, &waktu_baca);
if (rc == RTC_OK) {
    printf("%02d:%02d:%02d\n", waktu_baca.hours, waktu_baca.minutes, waktu_baca.seconds);
} else {
    printf("Read gagal! code: %d\n", rc);
}
```

---

#### `uint8_t dec_to_bcd(uint8_t val)` / `uint8_t bcd_to_dec(uint8_t val)`
Fungsi konversi internal yang diekspos untuk keperluan unit testing.

| Fungsi | Input | Output |
|---|---|---|
| `dec_to_bcd` | Desimal (0-99) | BCD (0x00-0x99) |
| `bcd_to_dec` | BCD (0x00-0x99) | Desimal (0-99) |

---
#### `void day_tracker_init(day_tracker_t *dt)`
Menginisialisasi struktur `day_tracker_t` ke kondisi awal (belum ada data pembanding). Wajib dipanggil sekali sebelum `day_tracker_check_rollover()` digunakan pertama kali.

| Parameter |           Deskripsi                                                         |
|-----------|-----------------------------------------------------------------------------|
|   `dt`    | Pointer ke struktur `day_tracker_t` yang akan direset (dialokasi pemanggil) |

**Perilaku:**
- `last_date`, `last_month`, `last_year` di-set ke `0`.
- `valid` di-set ke `0`, menandakan tracker belum punya data acuan.

**Contoh:**
```c
day_tracker_t tracker;
day_tracker_init(&tracker);
```

---

#### `int day_tracker_check_rollover(day_tracker_t *dt, i2c_t *i2c_handle)`
Membaca waktu terkini dari RTC lalu membandingkannya dengan data tanggal terakhir yang tersimpan di `dt`, untuk mendeteksi apakah hari sudah berganti (rollover).

|  Parameter   |    Deskripsi                                                                |
|--------------|-----------------------------------------------------------------------------|
|     `dt`     | Pointer ke struktur `day_tracker_t` yang menyimpan tanggal acuan sebelumnya |
| `i2c_handle` | Handle I2C yang diperoleh dari `rtc_init()`                                 |

|           Return            | Keterangan |
|-----------------------------|------------|
|           `1`               | Hari berganti (date/month/year berbeda dari data tersimpan sebelumnya) — `dt` otomatis diperbarui ke nilai terbaru |
|           `0`               | Belum ada pergantian hari, atau ini adalah pemanggilan pertama kali (`dt->valid == 0`) — nilai acuan langsung disimpan tanpa dianggap rollover |
|   `RTC_ERR_NULL_PTR` (-1)   | Parameter `dt` atau `i2c_handle` bernilai `NULL` |
| (kode error `rtc_get_time`) | Jika pembacaan RTC gagal, error code diteruskan apa adanya dari `rtc_get_time()` (mis. `RTC_ERR_I2C_WRITE_PTR`, `RTC_ERR_I2C_READ`, dst.) |

**Catatan Logika:**
- Pemanggilan pertama (`dt->valid == 0`) hanya menyimpan tanggal saat ini sebagai acuan dan mengembalikan `0`: bukan dianggap sebagai rollover.
- Pembandingan dilakukan pada tiga field sekaligus (`date`, `month`, `year`) sehingga pergantian bulan/tahun juga terdeteksi sebagai rollover, bukan hanya pergantian tanggal harian biasa.
- Fungsi ini melakukan pembacaan I2C penuh (`rtc_get_time`) setiap dipanggil, sehingga cocok dipanggil secara periodik (mis. tiap beberapa menit/jam), bukan tiap loop cepat, untuk menghindari beban bus I2C berlebih.

**Contoh:**
```c
day_tracker_t tracker;
day_tracker_init(&tracker);

// Dipanggil secara periodik di main loop
int rc = day_tracker_check_rollover(&tracker, rtc_handle);
if (rc == 1) {
    printf("Hari berganti! Tanggal baru: %02d-%02d-%02d\n",
           tracker.last_date, tracker.last_month, tracker.last_year);
} else if (rc < 0) {
    printf("Gagal cek rollover, error code: %d\n", rc);
}
```
---
## Catatan Penting: Konvensi Library I2C PULPissimo

Library `i2c.c` bawaan PULP Runtime memiliki konvensi khusus yang **berbeda** antara `i2c_write` dan `i2c_read`:

| Aspek | `i2c_write()` | `i2c_read()` |
|---|---|---|
| **Return sukses** | `0` | `length` (jumlah byte dibaca) |
| **Return gagal** | `5` (timeout) | `0` (timeout) |
| **Parameter ke-4** | `send_stop`: `1`=kirim STOP, `0`=tanpa STOP | `pending`: `0`=kirim STOP, `1`=tanpa STOP |
| **Address** | Otomatis dari `dev->cs` | Otomatis dari `dev->cs \| 0x1` |

> **Jangan** memasukkan address byte ke dalam buffer `data[]`. Library menangani pengiriman address secara otomatis melalui field `dev->cs`.

---

## Requirement Tree

```
rtc kalender
├── membaca kalender dan jam dari rtc
│   ├── bisa menggunakan i2c
│   └── bisa membaca register dari rtc
└── menyingkronkan data rtc ketika sistem menyala dan online
    ├── bisa menggunakan i2c
    └── bisa membaca register dari rtc
```

---

## Build & Flash

```bash
# Kompilasi untuk board FPGA
cd libraries/sensors/DS1307
bash build.sh

# Unit test lokal (tanpa hardware)
gcc -o test_host test_host.c && ./test_host
```

## Dependensi
- **PULP Runtime** (`pulp-runtime/`)
- **RISC-V GCC Toolchain** (`chroot/bin/`)
- **Target:** PULPissimo pada FPGA (10 MHz peripheral/SoC clock)

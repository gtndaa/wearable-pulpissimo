# NTP Sync - Modul Sinkronisasi Waktu Jaringan untuk RTC DS1307

Library untuk mengambil waktu akurat dari internet (server NTP) dan menyinkronkannya ke modul RTC DS1307 pada platform PULPissimo.

---

## Fitur

| Fitur | Deskripsi |
|-------|-----------|
| Sync 1x | Panggil `ntp_sync_now()` untuk ambil waktu dari internet dan set ke RTC |
| Set Manual | Panggil `ntp_manual_set()` untuk set waktu tanpa internet |
| Dual Mode | Mendukung Wi-Fi Laptop (testing) dan Ethernet Board (deployment) |
| Error Handling | Kode error spesifik (`-1` s/d `-7`) untuk debugging |

---

## Cara Compile dan Jalankan

### Mode Laptop (Testing via Wi-Fi)
```bash
cd libraries/app/ntp_sync
make
./test_ntp
```

### Mode Board (PULPissimo via Ethernet)
```bash
cd libraries/app/ntp_sync
source build.sh
```

---

## API Reference

### `int ntp_sync_now(i2c_t *rtc_handle)`
Mengambil waktu dari internet (NTP) dan langsung menyimpannya ke RTC. Dipanggil 1 kali saja.

| Parameter | Deskripsi |
|-----------|-----------|
| `rtc_handle` | Handle I2C yang diperoleh dari `rtc_init()` |

| Return | Keterangan |
|--------|------------|
| `0` (NTP_OK) | Sukses, waktu RTC sudah ter-update |
| `-1` s/d `-7` | Error (lihat tabel di bawah) |

**Contoh:**
```c
i2c_t *rtc_handle;
i2c_dev_t dev;
rtc_init(&dev, &rtc_handle);

int rc = ntp_sync_now(rtc_handle);
if (rc == NTP_OK) {
    printf("Jam sudah sinkron dengan internet!\n");
}
```

---

### `int ntp_manual_set(i2c_t *rtc_handle, rtc_time_t *waktu)`
Set waktu RTC secara manual tanpa internet.

**Contoh:**
```c
rtc_time_t waktu = {
    .hours = 14, .minutes = 30, .seconds = 0,
    .date = 14, .month = 7, .year = 26, .day = 1
};
ntp_manual_set(rtc_handle, &waktu);
```

---

## Error Codes

| Kode | Konstanta | Sumber | Artinya |
|------|-----------|--------|---------|
| `0` | `NTP_OK` | - | Sukses |
| `-1` | `NTP_ERR_SOCKET` | `ntp_network_connect()` | Gagal buka socket |
| `-2` | `NTP_ERR_DNS` | `ntp_network_connect()` | Gagal resolve DNS |
| `-3` | `NTP_ERR_CONNECT` | `ntp_network_connect()` | Gagal koneksi ke server |
| `-4` | `NTP_ERR_SEND` | `ntp_fetch_timestamp()` | Gagal kirim paket |
| `-5` | `NTP_ERR_TIMEOUT` | `ntp_fetch_timestamp()` | Timeout 5 detik |
| `-6` | `NTP_ERR_RTC_SET` | `time_sync_to_rtc()` | Gagal set RTC (I2C) |
| `-7` | `NTP_ERR_INVALID` | `time_sync_to_rtc()` | Data waktu tidak valid |

---

## Cara Mengubah Mode Laptop → Board (OTOMATIS!)

Kodingan ini **sudah 100% otomatis**! Anda **TIDAK PERLU menghapus atau mengubah apapun** di kodingan saat pindah dari Laptop ke Board.

Semuanya diatur otomatis dari cara Anda melakukan compile:
1. **Mode Laptop:** Saat Anda mengetik `gcc ... -DTEST_DI_LAPTOP`, saklar akan otomatis mengaktifkan library Wi-Fi Linux.
2. **Mode Board:** Saat Anda menjalankan `source ./build.sh`, saklar akan otomatis mengaktifkan library Ethernet PULPissimo (karena tidak ada flag `-DTEST_DI_LAPTOP`).

Tugas tim selanjutnya hanyalah mengisi bagian `#else` di Fungsi 1 dan 2 dengan library Ethernet bawaan board (LwIP). Fungsi 3 tidak perlu diubah sama sekali.

---

## ⚡ Versi STM32 (Sudah Berjalan di Hardware)

> **PENTING:** Karena jalur SPI di board PULPissimo masih ada bug, modul ini sudah di-porting dan **berhasil dijalankan di STM32 Nucleo F401RE** sebagai solusi sementara. Semua source code STM32 ada di folder `test_stm32/`.

### Cara Menjalankan Versi STM32

1. Buka folder `test_stm32/` menggunakan **VSCode + PlatformIO**.
2. Hubungkan board STM32 Nucleo via USB.
3. Hubungkan modul W5500 ke kabel LAN (router).
4. Klik **Upload (→)** di PlatformIO, lalu buka **Serial Monitor**.

### Wiring STM32 ↔ W5500 (SPI)

| Pin STM32 | Pin W5500 | Fungsi |
|-----------|-----------|--------|
| D13 | SCK | Clock |
| D12 | MISO | Data masuk |
| D11 | MOSI | Data keluar |
| D10 | SCS | Chip Select |

### Wiring STM32 ↔ DS1307 (I2C)

| Pin STM32 | Pin DS1307 | Fungsi |
|-----------|------------|--------|
| D14 (SDA) | SDA | Data |
| D15 (SCL) | SCL | Clock |

### API STM32 (C++ / Arduino)

```cpp
#include "ntp_sync.h"

RTC_DS1307 rtc;

void setup() {
    Serial.begin(115200);

    // 1. Inisialisasi hardware (W5500 + DS1307), otomatis DHCP
    ntp_init(rtc);

    // 2. Sinkronkan jam dari internet
    if (ntp_sync_now(rtc) != NTP_OK) {
        // Fallback manual jika internet mati
        ntp_manual_set(rtc, 26, 7, 17, 12, 0, 0);
    }
}

void loop() {
    // 3. Baca jam dari RTC setiap detik
    ntp_print_rtc_time(rtc);
    delay(1000);
}
```

### Konfigurasi yang Bisa Diubah (di `test_stm32/lib/ntp_sync/ntp_sync.h`)

| Setting | Default | Keterangan |
|---------|---------|------------|
| `W5500_CS_PIN` | `10` | Pin Chip Select W5500 |
| `BOARD_IP` | `192.168.1.111` | IP statis cadangan (jika DHCP gagal) |
| `NTP_UTC_OFFSET_WIB` | `25200` | Zona waktu (7 × 3600 = UTC+7) |

### Error Codes STM32

| Kode | Artinya |
|------|---------|
| `-4` | Gagal kirim paket (cek kabel LAN) |
| `-5` | Timeout (internet lambat / server down) |
| `-8` | W5500 tidak terdeteksi (cek kabel SPI, awas bus contention!) |
| `-9` | RTC DS1307 tidak terdeteksi (cek kabel I2C) |

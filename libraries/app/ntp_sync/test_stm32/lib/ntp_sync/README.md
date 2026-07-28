# NTP Sync Module untuk STM32

Modul ini adalah *library* C++ khusus untuk mengambil waktu dari internet (NTP Server) menggunakan modul Ethernet **W5500** (via SPI) dan menyimpannya secara permanen ke Modul RTC **DS1307** (via I2C). 

Saat ini, modul ini berjalan di atas framework **Arduino/PlatformIO** untuk board STM32 Nucleo.

---

## 🛠️ Cara Mengubah Konfigurasi (Customization)
Jika Anda (atau anak magang selanjutnya) ingin menyesuaikan library ini dengan jaringan atau hardware yang berbeda, semua pengaturan terpusat di file **`ntp_sync.h`**.

1. **Ubah Pin CS (Chip Select) W5500**
   Secara default, CS berada di Pin D10. Jika desain PCB berubah, cukup ganti baris ini:
   ```cpp
   #define W5500_CS_PIN 10
   ```

2. **Pengaturan IP Address (DHCP vs Statis)**
   Library ini secara otomatis akan mencari IP menggunakan **DHCP**. Anda tidak perlu repot menyetting IP router. 
   Namun, jika DHCP gagal, ia akan menggunakan IP statis cadangan. Anda bisa mengubah IP statis ini di `ntp_sync.h`:
   ```cpp
   #define BOARD_IP      IPAddress(192, 168, 1, 111) // Ganti sesuai IP router Anda
   #define BOARD_GATEWAY IPAddress(192, 168, 1, 1)
   ```

3. **Zona Waktu (Timezone)**
   Default menggunakan WIB (UTC+7 = 7 * 3600 detik = 25200). Jika device akan dipakai di zona waktu lain:
   ```cpp
   #define NTP_UTC_OFFSET_WIB 25200
   ```

---

## 🔄 Catatan Migrasi ke PULPissimo
Saat ini modul ditulis dalam **C++ (Arduino)** untuk **STM32** karena masalah hardware SPI di PULPissimo. 

**Jika suatu saat hardware PULPissimo sudah beres dan ingin dipindah kembali ke sana:**
1. File ini sangat modular. Inti logika NTP (parsing byte array, aritmatika epoch) tidak perlu diubah sama sekali.
2. Anda **hanya perlu mengganti 2 hal**:
   - Ganti pemanggilan `RTClib` (I2C) kembali ke fungsi I2C bawaan PULPissimo (`i2c_write()`).
   - Ganti pemanggilan `EthernetUDP` kembali ke soket TCP/IP bawaan PULPissimo (`lwip` / socket standar C).

---

## 🔌 Kebutuhan Hardware & Wiring (Versi STM32)

- **Ethernet W5500:** `SCK` ➔ D13, `MISO` ➔ D12, `MOSI` ➔ D11, `CS` ➔ D10
- **RTC DS1307:** `SDA` ➔ D14, `SCL` ➔ D15

---

## 📚 API Reference

### 1. `ntp_init(RTC_DS1307 &rtc)`
- Menginisialisasi W5500 & DS1307. Otomatis meminta IP dari router (DHCP).
- **Return:** `NTP_OK` (0) jika sukses, atau kode error negatif (`-8` = W5500 gagal deteksi).

### 2. `ntp_sync_now(RTC_DS1307 &rtc)`
- Mengambil waktu dari Cloudflare NTP (162.159.200.1), merubah ke WIB, & menyimpan ke RTC.
- **Return:** `NTP_OK` (0) jika sukses.

### 3. `ntp_print_rtc_time(RTC_DS1307 &rtc)`
- Membaca waktu dari RTC dan menampilkannya di Serial Monitor. (Contoh: `[RTC] 17/07/2026 12:00:00`).

### 4. `ntp_manual_set(...)`
- Memasukkan waktu ke RTC secara manual jika tidak ada internet.

---

## 🚀 Contoh Penggunaan (main.cpp)

```cpp
#include <Arduino.h>
#include "ntp_sync.h"

RTC_DS1307 rtc; // Buat instance RTC

void setup() {
    Serial.begin(115200);
    
    // 1. Nyalakan Hardware & Dapatkan IP
    if (ntp_init(rtc) == NTP_OK) {
        
        // 2. Sinkronkan Waktu dengan Internet
        if (ntp_sync_now(rtc) == NTP_OK) {
            Serial.println("Berhasil sinkron dengan NTP Server!");
        } else {
            Serial.println("Gagal internet. Set jam manual...");
            ntp_manual_set(rtc, 26, 7, 17, 12, 0, 0); // 17 Juli 2026, 12:00:00
        }
    }
}

void loop() {
    // 3. Pantau jam setiap 1 detik
    ntp_print_rtc_time(rtc);
    delay(1000);
}
```

## ⚠️ Daftar Kode Error
- `-4`: Gagal kirim paket (Cek kabel LAN / beda segmen IP).
- `-5`: Timeout NTP Server (Internet lambat / terblokir).
- `-8`: W5500 tidak terdeteksi (Cek kabel SPI, awas bus contention/tabrakan pin CS!).

# Dokumentasi API — AFE4400 PPG Passthrough

Driver untuk membaca data sensor PPG dari **AFE4400** melalui **SPI** pada platform **PULPissimo**.

## Struktur Library

- **`afe4400.h`** — Definisi register dan deklarasi fungsi API.
- **`afe4400.c`** — Implementasi driver.
- **`test_ppg.c`** — Unit test inisialisasi, kalibrasi, dan pembacaan data passthrough.

## Register Data AFE4400

Register yang digunakan untuk membaca data PPG passthrough (sesuai Datasheet Table 6):

| Nama | Alamat | Deskripsi |
|------|--------|-----------|
| `AFE_CONTROL0` | `0x00` | Kontrol SW Reset dan mode SPI |
| `AFE_LEDCNTRL` | `0x22` | Pengaturan arus LED1 dan LED2 |
| `AFE_LED1VAL`  | `0x2C` | Nilai ADC 24-bit LED1 (Merah) |
| `AFE_LED2VAL`  | `0x2A` | Nilai ADC 24-bit LED2 (Inframerah) |
| `AFE_ALED1VAL` | `0x2D` | Nilai ADC 24-bit ambient saat LED1 mati |

## Referensi Fungsi

### `afe4400_init`
```c
int afe4400_init(int spi_id, int cs_pin);
```
Membuka jalur SPI dan melakukan software reset pada sensor.
- `spi_id` : Nomor jalur SPI, gunakan `0` untuk SPI_0.
- `cs_pin` : Nomor pin Chip Select.
- **Return** : `0` jika berhasil, `-1` jika gagal.

### `afe4400_calibrate`
```c
void afe4400_calibrate(void);
```
Mengatur arus LED ke nilai default yang aman (~6 mA) agar sensor siap membaca.
Dipanggil satu kali setelah `afe4400_init()`.

### `afe4400_read_reg`
```c
unsigned int afe4400_read_reg(unsigned char reg_addr);
```
Membaca nilai 24-bit dari register ADC sensor.
- `reg_addr` : Alamat register, gunakan konstanta `AFE_LED1VAL`, `AFE_LED2VAL`, `AFE_ALED1VAL`.
- **Return** : Nilai raw ADC 24-bit dari sensor.

### `afe4400_write_reg`
```c
void afe4400_write_reg(unsigned char reg_addr, unsigned int data);
```
Menulis nilai konfigurasi ke register sensor. Digunakan secara internal oleh `afe4400_init()` dan `afe4400_calibrate()`.

## Contoh Penggunaan

```c
#include "afe4400.h"
#include <stdio.h>

int main() {
    if (afe4400_init(0, 0) != 0) {
        printf("[ERROR] Inisialisasi gagal!\n");
        return -1;
    }

    afe4400_calibrate();

    for (int i = 1; i <= 10; i++) {
        unsigned int led1 = afe4400_read_reg(AFE_LED1VAL);
        unsigned int led2 = afe4400_read_reg(AFE_LED2VAL);
        unsigned int amb  = afe4400_read_reg(AFE_ALED1VAL);
        printf("%2d | LED1: %u | LED2: %u | Ambient: %u\n", i, led1, led2, amb);
    }
    return 0;
}

void pe_start(void) {}
```

---
## Cara Menjalankan Unit Test
**Di laptop (simulasi, tanpa hardware):**
```bash
cd ~/ICDEC/wearable-pulpissimo/libraries/sensors/afe4400
gcc -DSOFTWARE_TEST test_ppg.c afe4400.c -o test_program && ./test_program
```
**Di hardware PULPissimo (FPGA):**
```bash
export PATH="$PATH:~/ICDEC/chroot/bin"
. ~/ICDEC/pulp-runtime/configs/pulpissimo.sh
. ~/ICDEC/pulp-runtime/configs/fpgas/pulpissimo/genesys2.sh
export PULPRT_CONFIG_CFLAGS="-DARCHI_FPGA_PER_FREQUENCY=10000000 -DARCHI_FPGA_SOC_FREQUENCY=10000000"
cd ~/ICDEC/wearable-pulpissimo/libraries/sensors/afe4400
make
echo "$?"   # 0 = berhasil
```
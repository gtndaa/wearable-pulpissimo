#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include "pulp.h" 

typedef struct {
    uint8_t seconds; // 0-59
    uint8_t minutes; // 0-59
    uint8_t hours;   // 0-23 (Mode 24 jam)
    uint8_t day;     // 1-7 (Hari dalam seminggu)
    uint8_t date;    // 1-31 (Tanggal)
    uint8_t month;   // 1-12 (Bulan)
    uint8_t year;    // 0-99 (Tahun)
} rtc_time_t;

// ===== Error Codes =====
#define RTC_OK                   0   // Sukses, tidak ada error
#define RTC_ERR_NULL_PTR        -1   // Parameter NULL (pointer tidak valid)
#define RTC_ERR_I2C_OPEN        -2   // Gagal membuka peripheral I2C (i2c_open)
#define RTC_ERR_VALIDATION      -3   // Nilai waktu diluar range (cek datasheet DS1307)
#define RTC_ERR_I2C_WRITE_SET   -4   // Gagal i2c_write saat SET time (kirim 9 byte)
#define RTC_ERR_I2C_WRITE_PTR   -5   // Gagal i2c_write saat set register pointer (tahap 1 GET)
#define RTC_ERR_I2C_WRITE_READ  -6   // Gagal i2c_write address READ (tahap 2 GET)
#define RTC_ERR_I2C_READ        -7   // Gagal i2c_read burst 7 byte (tahap 2 GET)

// Fungsi untuk inisialisasi hardware I2C_0 ke DS1307
int rtc_init(i2c_dev_t *dev_conf, i2c_t **i2c_handle);

// Fungsi untuk membaca waktu desimal dari RTC DS1307
int rtc_get_time(i2c_t *i2c_handle, rtc_time_t *time);

// Fungsi untuk mengatur waktu awal sekaligus mengaktifkan jam (clear bit CH)
int rtc_set_time(i2c_t *i2c_handle, rtc_time_t *time);

// Fungsi konversi BCD untuk unit testing
uint8_t dec_to_bcd(uint8_t val);
uint8_t bcd_to_dec(uint8_t val);

// Fungsi Day Tracker untuk mendeteksi pergantian hari (rollover) dari RTC DS1307
typedef struct {
    uint8_t last_date;
    uint8_t last_month;
    uint8_t last_year;
    int     valid;
} day_tracker_t;

void day_tracker_init(day_tracker_t *dt);
int day_tracker_check_rollover(day_tracker_t *dt, i2c_t *i2c_handle);

#endif 

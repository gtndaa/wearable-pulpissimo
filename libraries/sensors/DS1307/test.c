
#include <stdio.h>
#include "pulp.h"
#include "rtc.h"

// UNIT TEST RTC DS1307 - UNTUK RISC-V BOARD (FPGA)

// PULP Runtime entry point - required by crt0
void pe_start(void);
int main(void);

// Helper: cetak pesan error detail berdasarkan error code dari rtc driver
static void print_rtc_error(int err) {
    switch (err) {
        case RTC_ERR_NULL_PTR:
            printf("  -> NULL pointer (handle/struct belum diinisialisasi)\n\r");
            break;
        case RTC_ERR_I2C_OPEN:
            printf("  -> i2c_open() gagal (cek koneksi I2C_0 / clock)\n\r");
            break;
        case RTC_ERR_VALIDATION:
            printf("  -> Nilai waktu diluar range (cek seconds/minutes/hours/day/date/month/year)\n\r");
            break;
        case RTC_ERR_I2C_WRITE_SET:
            printf("  -> i2c_write() gagal saat SET time (kirim 9 byte ke DS1307)\n\r");
            printf("     Kemungkinan: DS1307 tidak ACK, kabel SDA/SCL putus, address salah\n\r");
            break;
        case RTC_ERR_I2C_WRITE_PTR:
            printf("  -> i2c_write() gagal saat set register pointer (tahap 1 GET)\n\r");
            printf("     Kemungkinan: DS1307 tidak merespon, bus I2C hang\n\r");
            break;
        case RTC_ERR_I2C_WRITE_READ:
            printf("  -> i2c_write() gagal saat kirim address READ (tahap 2 GET)\n\r");
            printf("     Kemungkinan: NACK dari DS1307, repeated start gagal\n\r");
            break;
        case RTC_ERR_I2C_READ:
            printf("  -> i2c_read() gagal saat burst read 7 byte (tahap 2 GET)\n\r");
            printf("     Kemungkinan: DS1307 tidak kirim data, timeout I2C\n\r");
            break;
        default:
            printf("  -> Error tidak dikenal (code: %d)\n\r", err);
            break;
    }
}

void pe_start(void) {
    //main();
}

int main(void) {
    int rc;  // return code untuk cek error

    printf("\n");
    printf("RTC DS1307 Unit Test - RISC-V BOARD\n");
    printf("\nInisialisasi I2C Bus\n\r");
    
    i2c_dev_t rtc_dev_conf;
    i2c_t *rtc_handle;

    printf("Menginisialisasi I2C_0...\n\r");
    rc = rtc_init(&rtc_dev_conf, &rtc_handle);
    if (rc != RTC_OK) {
        printf("[ERROR] I2C initialization failed! (code: %d)\n\r", rc);
        print_rtc_error(rc);
        return -1;
    }
    printf("[OK] I2C Initialized!\n\r");
    printf("\n Set Waktu Awal \n\r");
    
    rtc_time_t waktu_awal = {
        .seconds = 0,
        .minutes = 30,
        .hours = 14,
        .day = 4,      // Kamis
        .date = 24,
        .month = 6,
        .year = 26
    };

    printf("Setting initial time: %02d:%02d:%02d | %02d/%02d/20%02d\n",
           waktu_awal.hours, waktu_awal.minutes, waktu_awal.seconds,
           waktu_awal.date, waktu_awal.month, waktu_awal.year);
    
    rc = rtc_set_time(rtc_handle, &waktu_awal);
    if (rc != RTC_OK) {
        printf("[ERROR] Failed to set RTC time! (code: %d)\n\r", rc);
        print_rtc_error(rc);
    } else {
        printf("[OK] RTC time set successfully!\n\r");
    }

    printf("\n Reading RTC Time\n\r");
    
    rtc_time_t waktu_baca = {0};
    int reg_errors;  // counter register error per iterasi

    // Timing: ukur presisi delay menggunakan hardware timer
    long t_now, t_prev = 0;
    long dt_us, miss_us;
    
    for (int i = 0; i < 10; i++) {
        // Catat waktu sebelum baca I2C
        t_now = pos_tick_get_counter_us();

        rc = rtc_get_time(rtc_handle, &waktu_baca);
        if (rc == RTC_OK) {
            // Print waktu RTC
            if (i == 0) {
                // Iterasi pertama: belum ada delta
                printf("[%02d] %02d:%02d:%02d | %02d/%02d/20%02d\n\r",
                       i+1,
                       waktu_baca.hours, waktu_baca.minutes, waktu_baca.seconds,
                       waktu_baca.date, waktu_baca.month, waktu_baca.year);
            } else {
                // Iterasi 2+: hitung delta dan miss dari ideal 1.000.000 us
                dt_us = t_now - t_prev;
                miss_us = dt_us - 1000000;
                long miss_ms = miss_us / 1000;
                printf("[%02d] %02d:%02d:%02d | %02d/%02d/20%02d | dt:%ldus miss:%c%ldus (%c%ldms)\n\r",
                       i+1,
                       waktu_baca.hours, waktu_baca.minutes, waktu_baca.seconds,
                       waktu_baca.date, waktu_baca.month, waktu_baca.year,
                       dt_us,
                       (miss_us >= 0) ? '+' : '-', (miss_us >= 0) ? miss_us : -miss_us,
                       (miss_ms >= 0) ? '+' : '-', (miss_ms >= 0) ? miss_ms : -miss_ms);
            }
            t_prev = t_now;

            // ===== Validasi per-register (cek range sesuai datasheet DS1307) =====
            reg_errors = 0;

            if (waktu_baca.seconds > 59) {
                printf("  [WARN] Reg 0x00 SECONDS = %d (valid: 0-59)\n\r", waktu_baca.seconds);
                reg_errors++;
            }
            if (waktu_baca.minutes > 59) {
                printf("  [WARN] Reg 0x01 MINUTES = %d (valid: 0-59)\n\r", waktu_baca.minutes);
                reg_errors++;
            }
            if (waktu_baca.hours > 23) {
                printf("  [WARN] Reg 0x02 HOURS = %d (valid: 0-23)\n\r", waktu_baca.hours);
                reg_errors++;
            }
            if (waktu_baca.day < 1 || waktu_baca.day > 7) {
                printf("  [WARN] Reg 0x03 DAY = %d (valid: 1-7)\n\r", waktu_baca.day);
                reg_errors++;
            }
            if (waktu_baca.date < 1 || waktu_baca.date > 31) {
                printf("  [WARN] Reg 0x04 DATE = %d (valid: 1-31)\n\r", waktu_baca.date);
                reg_errors++;
            }
            if (waktu_baca.month < 1 || waktu_baca.month > 12) {
                printf("  [WARN] Reg 0x05 MONTH = %d (valid: 1-12)\n\r", waktu_baca.month);
                reg_errors++;
            }
            if (waktu_baca.year > 99) {
                printf("  [WARN] Reg 0x06 YEAR = %d (valid: 0-99)\n\r", waktu_baca.year);
                reg_errors++;
            }

            if (reg_errors > 0) {
                printf("  [!] %d register diluar range! Kemungkinan: data corrupt, BCD error, atau baterai DS1307 mati\n\r", reg_errors);
            }

        } else {
            printf("[%02d] ERROR reading RTC! (code: %d)\n\r", i+1, rc);
            print_rtc_error(rc);
            t_prev = t_now;
        }

        // Delay akurat 1 detik menggunakan hardware timer PULP
        pos_delay_busy_ms(1000);
    }

    printf("\nTest completed successfully!\n\r");
    
    return 0;
}


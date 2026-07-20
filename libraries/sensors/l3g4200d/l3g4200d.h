/*
 * Copyright (C) 2026 ICDeC
 *
 * Driver library minimal untuk STMicroelectronics L3G4200D 3-Axis Gyroscope.
 *
 * Library ini adalah hasil reverse-engineering dari raw I2C test yang
 * sudah TERBUKTI berhasil di board ICDeC PULPissimo (Nusacore FPGA).
 * Hanya berisi register yang benar-benar dipakai -- bukan port lengkap
 * dari seluruh peta register L3G4200D.
 *
 * CATATAN PENTING (temuan dari debugging):
 *   1. PULPissimo TIDAK punya FPU hardware -- semua konversi ke deg/s
 *      dilakukan dengan integer fixed-point (dps * 100), BUKAN float.
 *   2. Perlu delay kecil antara fase "tulis alamat register" dan fase
 *      "baca data" (repeated-start) -- tanpa ini, hasil baca selalu 0xFF.
 *      Delay ini sudah dibangun ke dalam l3g4200d_read_reg() internal.
 *   3. Output data L3G4200D LITTLE-ENDIAN (byte Low duluan, baru High).
 *   4. Untuk multi-byte read, bit MSB (0x80) alamat register WAJIB di-set
 *      supaya auto-increment aktif.
 */

#ifndef __L3G4200D_H__
#define __L3G4200D_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Status Codes
 * ============================================================================ */

typedef enum {
    L3G4200D_OK          = 0,   /**< Operasi berhasil */
    L3G4200D_ERR_I2C     = -1,  /**< I2C gagal buka / komunikasi umum */
    L3G4200D_ERR_ID      = -2,  /**< WHO_AM_I tidak sesuai (bukan L3G4200D) */
    L3G4200D_ERR_CONFIG  = -3,  /**< Gagal konfigurasi register */
    L3G4200D_ERR_TIMEOUT = -4,  /**< Transaksi I2C timeout */
    L3G4200D_ERR_NULL    = -5   /**< Pointer argumen NULL */
} l3g4200d_status_t;

/* ============================================================================
 * I2C Address
 * ============================================================================ */

#define L3G4200D_I2C_ADDR_DEFAULT   0x69   /**< SDO pin = HIGH (Vdd) */
#define L3G4200D_I2C_ADDR_ALT       0x68   /**< SDO pin = LOW (GND) */

/* ============================================================================
 * Register Map (HANYA yang dipakai library ini)
 * ============================================================================ */

#define L3G4200D_REG_WHO_AM_I       0x0F   /**< Device ID, read-only, default 0xD3 */
#define L3G4200D_REG_CTRL_REG1     0x20   /**< ODR, bandwidth, power mode, axis enable */
#define L3G4200D_REG_CTRL_REG4     0x23   /**< Full-scale range selection */
#define L3G4200D_REG_OUT_X_L       0x28   /**< Awal data gyro 16-bit x3 axis (6 byte berurutan) */

#define L3G4200D_AUTO_INCREMENT_BIT 0x80  /**< OR-kan ke alamat register saat baca >1 byte */

#define L3G4200D_WHO_AM_I_VALUE     0xD3  /**< Nilai WHO_AM_I yang benar */

/* ============================================================================
 * CTRL_REG1 value siap pakai
 * ============================================================================ */

#define L3G4200D_CTRL_REG1_NORMAL_XYZ_100HZ  0x0F  /**< ODR=100Hz, PD=1 (normal), X/Y/Z enable */
#define L3G4200D_CTRL_REG1_POWER_DOWN        0x00  /**< Power-down mode */

/* ============================================================================
 * Full-Scale Range (CTRL_REG4)
 * ============================================================================ */

typedef enum {
    L3G4200D_RANGE_250DPS  = 0x00,  /**< +/-250 dps  (sensitivitas 8.75 mdps/digit) */
    L3G4200D_RANGE_500DPS  = 0x10,  /**< +/-500 dps  (sensitivitas 17.50 mdps/digit) */
    L3G4200D_RANGE_2000DPS = 0x20   /**< +/-2000 dps (sensitivitas 70.00 mdps/digit) */
} l3g4200d_range_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/** Data mentah gyro (16-bit signed, langsung dari register). */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} l3g4200d_raw_t;

/**
 * Data gyro dalam deg/s, format FIXED-POINT (dikali 100).
 * Contoh: x = 875 artinya 8.75 dps.
 * Dipakai KARENA PULPissimo tidak punya FPU -- pakai integer supaya
 * hasil selalu benar dan cepat.
 */
typedef struct {
    int32_t x_x100;
    int32_t y_x100;
    int32_t z_x100;
} l3g4200d_dps_x100_t;

/**
 * Data gyro dalam deg/s, format FLOAT.
 * Dipakai untuk menguji apakah FPU hardware tersedia di sintesis saat ini.
 * PERINGATAN: Hanya gunakan ini jika core disintesis DENGAN ekstensi 'F'
 * (FPU). Tanpa FPU hardware, operasi float akan menghasilkan illegal
 * instruction atau nilai yang salah.
 */
typedef struct {
    float x;
    float y;
    float z;
} l3g4200d_dps_float_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

typedef struct {
    uint8_t          i2c_addr;   /**< 7-bit I2C address (0x69 atau 0x68) */
    int              i2c_id;     /**< I2C peripheral ID (biasanya 0) */
    unsigned int     i2c_freq;   /**< Baudrate I2C (disarankan 100000 / 100kHz) */
    l3g4200d_range_t range;      /**< Full-scale range awal */
} l3g4200d_config_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/** Isi cfg dengan nilai default yang sudah terbukti bekerja (addr=0x69, 100kHz, +/-250dps). */
l3g4200d_status_t l3g4200d_default_config(l3g4200d_config_t *cfg);

/**
 * Inisialisasi sensor: buka I2C, coba alamat default (0x69) lalu alamat
 * alternatif (0x68) kalau gagal, verifikasi WHO_AM_I, lalu konfigurasi
 * CTRL_REG1 (power up + enable XYZ) dan CTRL_REG4 (full-scale range).
 */
l3g4200d_status_t l3g4200d_init(l3g4200d_config_t *cfg);

/** Baca ulang WHO_AM_I (untuk verifikasi manual). */
l3g4200d_status_t l3g4200d_who_am_i(uint8_t *id);

/** Baca data gyro mentah (raw, 16-bit signed per axis). */
l3g4200d_status_t l3g4200d_read_raw(l3g4200d_raw_t *raw);

/** Baca data gyro dan konversi ke deg/s (format fixed-point x100). */
l3g4200d_status_t l3g4200d_read_dps_x100(l3g4200d_dps_x100_t *dps);

/** Baca data gyro dan konversi ke deg/s (format FLOAT -- untuk uji FPU). */
l3g4200d_status_t l3g4200d_read_dps_float(l3g4200d_dps_float_t *dps);

/** Ganti full-scale range saat runtime. */
l3g4200d_status_t l3g4200d_set_range(l3g4200d_range_t range);

/** Power-down sensor dan tutup I2C. */
l3g4200d_status_t l3g4200d_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __L3G4200D_H__ */
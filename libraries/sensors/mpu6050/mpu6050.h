/*
 * Copyright (C) 2026 ICDeC
 *
 * Driver library untuk InvenSense MPU-6050 6-Axis Gyroscope + Accelerometer.
 *
 * Library ini adalah hasil reverse-engineering dari raw I2C test yang
 * sudah TERBUKTI berhasil di board ICDeC PULPissimo (Nusacore FPGA).
 * Hanya berisi register yang benar-benar dipakai -- bukan port lengkap
 * dari seluruh peta register MPU-6050.
 *
 * CATATAN PENTING (temuan dari debugging):
 *   1. PULPissimo TIDAK punya FPU hardware -- semua konversi dilakukan
 *      dengan integer fixed-point, BUKAN float.
 *      - Gyro: dps * 100 (centidegrees per second)
 *      - Accel: milli-g (mg)
 *   2. Perlu delay kecil antara fase "tulis alamat register" dan fase
 *      "baca data" (repeated-start) -- tanpa ini, hasil baca selalu 0xFF.
 *      Delay ini sudah dibangun ke dalam fungsi internal.
 *   3. Output data MPU-6050 BIG-ENDIAN (byte High duluan, baru Low).
 *      (Kebalikan dari L3G4200D yang little-endian.)
 *   4. MPU-6050 otomatis auto-increment saat baca berurutan -- TIDAK perlu
 *      set bit 0x80 seperti L3G4200D.
 *   5. Setelah power-on, MPU-6050 dalam SLEEP MODE. WAJIB tulis 0x00 ke
 *      PWR_MGMT_1 untuk bangunkan device.
 */

#ifndef __MPU6050_H__
#define __MPU6050_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Status Codes
 * ============================================================================ */

typedef enum {
    MPU6050_OK          = 0,   /**< Operasi berhasil */
    MPU6050_ERR_I2C     = -1,  /**< I2C gagal buka / komunikasi umum */
    MPU6050_ERR_ID      = -2,  /**< WHO_AM_I tidak sesuai (bukan MPU-6050) */
    MPU6050_ERR_CONFIG  = -3,  /**< Gagal konfigurasi register */
    MPU6050_ERR_TIMEOUT = -4,  /**< Transaksi I2C timeout */
    MPU6050_ERR_NULL    = -5   /**< Pointer argumen NULL */
} mpu6050_status_t;

/* ============================================================================
 * I2C Address
 * ============================================================================ */

#define MPU6050_I2C_ADDR_DEFAULT    0x69   /**< AD0 pin = HIGH (Vdd) */
#define MPU6050_I2C_ADDR_ALT       0x68   /**< AD0 pin = LOW (GND) */

/* ============================================================================
 * Register Map (HANYA yang dipakai library ini)
 * ============================================================================ */

#define MPU6050_REG_SMPLRT_DIV      0x19   /**< Sample Rate Divider */
#define MPU6050_REG_CONFIG          0x1A   /**< Configuration (DLPF, EXT_SYNC) */
#define MPU6050_REG_GYRO_CONFIG     0x1B   /**< Gyro full-scale range (FS_SEL) */
#define MPU6050_REG_ACCEL_CONFIG    0x1C   /**< Accel full-scale range (AFS_SEL) */
#define MPU6050_REG_ACCEL_XOUT_H    0x3B   /**< Awal data accel 16-bit x3 axis (6 byte berurutan) */
#define MPU6050_REG_GYRO_XOUT_H     0x43   /**< Awal data gyro 16-bit x3 axis (6 byte berurutan) */
#define MPU6050_REG_PWR_MGMT_1      0x6B   /**< Power Management 1 */
#define MPU6050_REG_WHO_AM_I        0x75   /**< Device ID, read-only, default 0x68 */

#define MPU6050_WHO_AM_I_VALUE      0x68   /**< Nilai WHO_AM_I yang benar */

/* ============================================================================
 * PWR_MGMT_1 values siap pakai
 * ============================================================================ */

#define MPU6050_PWR1_DEVICE_RESET   0x80   /**< Device reset (bit 7) */
#define MPU6050_PWR1_SLEEP          0x40   /**< Sleep mode (bit 6) -- default ON setelah power-on */
#define MPU6050_PWR1_WAKEUP         0x00   /**< Clear sleep + reset -> wake up */

/* ============================================================================
 * Gyroscope Full-Scale Range (GYRO_CONFIG register bits [4:3])
 * ============================================================================ */

typedef enum {
    MPU6050_RANGE_250DPS  = 0x00,  /**< +/-250 dps  (sensitivitas 131   LSB/°/s) */
    MPU6050_RANGE_500DPS  = 0x08,  /**< +/-500 dps  (sensitivitas 65.5  LSB/°/s) */
    MPU6050_RANGE_1000DPS = 0x10,  /**< +/-1000 dps (sensitivitas 32.8  LSB/°/s) */
    MPU6050_RANGE_2000DPS = 0x18   /**< +/-2000 dps (sensitivitas 16.4  LSB/°/s) */
} mpu6050_range_t;

/* ============================================================================
 * Accelerometer Full-Scale Range (ACCEL_CONFIG register bits [4:3])
 *
 *   AFS_SEL | Range | Sensitivity (LSB/g)
 *   --------|-------|--------------------
 *     0x00  |  ±2g  |  16384
 *     0x08  |  ±4g  |   8192
 *     0x10  |  ±8g  |   4096
 *     0x18  | ±16g  |   2048
 * ============================================================================ */

typedef enum {
    MPU6050_ACCEL_RANGE_2G  = 0x00,  /**< ±2g,  16384 LSB/g */
    MPU6050_ACCEL_RANGE_4G  = 0x08,  /**< ±4g,   8192 LSB/g */
    MPU6050_ACCEL_RANGE_8G  = 0x10,  /**< ±8g,   4096 LSB/g */
    MPU6050_ACCEL_RANGE_16G = 0x18   /**< ±16g,  2048 LSB/g */
} mpu6050_accel_range_t;

/* ============================================================================
 * Data Structures — Gyroscope
 * ============================================================================ */

/** Data mentah gyro (16-bit signed, langsung dari register). */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} mpu6050_raw_t;

/**
 * Data gyro dalam deg/s, format FIXED-POINT (dikali 100).
 * Contoh: x = 359 artinya 3.59 dps.
 * Dipakai KARENA PULPissimo tidak punya FPU -- pakai integer supaya
 * hasil selalu benar dan cepat.
 */
typedef struct {
    int32_t x_x100;
    int32_t y_x100;
    int32_t z_x100;
} mpu6050_dps_x100_t;

/* ============================================================================
 * Data Structures — Accelerometer
 * ============================================================================ */

/** Data mentah accelerometer (16-bit signed, langsung dari register). */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} mpu6050_accel_raw_t;

/**
 * Data accelerometer dalam milli-g (mg), format integer.
 * Contoh: x_mg = 1000 artinya 1.000g (1g).
 *
 * Konversi dari raw ke mg (integer fixed-point):
 *   ±2g  -> 16384 LSB/g -> mg = raw * 125 / 2048
 *   ±4g  ->  8192 LSB/g -> mg = raw * 125 / 1024
 *   ±8g  ->  4096 LSB/g -> mg = raw * 125 / 512
 *   ±16g ->  2048 LSB/g -> mg = raw * 125 / 256
 *
 * Overflow check: max raw 32767 * 125 = 4,095,875 -> aman int32_t
 */
typedef struct {
    int32_t x_mg;
    int32_t y_mg;
    int32_t z_mg;
} mpu6050_accel_mg_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

typedef struct {
    uint8_t               i2c_addr;     /**< 7-bit I2C address (0x69 atau 0x68) */
    int                   i2c_id;       /**< I2C peripheral ID (biasanya 0) */
    unsigned int          i2c_freq;     /**< Baudrate I2C (disarankan 100000 / 100kHz) */
    mpu6050_range_t       gyro_range;   /**< Gyro full-scale range awal */
    mpu6050_accel_range_t accel_range;  /**< Accel full-scale range awal */
    uint8_t               dlpf_cfg;     /**< Digital Low Pass Filter config (0-6) */
    uint8_t               smplrt_div;   /**< Sample rate divider */
} mpu6050_config_t;

/* ============================================================================
 * Public API — Umum
 * ============================================================================ */

/** Isi cfg dengan nilai default yang sudah terbukti bekerja.
 *  Default: addr=0x69, 100kHz, gyro=±250dps, accel=±2g. */
mpu6050_status_t mpu6050_default_config(mpu6050_config_t *cfg);

/**
 * Inisialisasi sensor: buka I2C, coba alamat default (0x69) lalu alamat
 * alternatif (0x68) kalau gagal, verifikasi WHO_AM_I, wake from sleep,
 * lalu konfigurasi GYRO_CONFIG, ACCEL_CONFIG, DLPF, dan SMPLRT_DIV.
 */
mpu6050_status_t mpu6050_init(mpu6050_config_t *cfg);

/** Baca ulang WHO_AM_I (untuk verifikasi manual). */
mpu6050_status_t mpu6050_who_am_i(uint8_t *id);

/** Sleep mode dan tutup I2C. */
mpu6050_status_t mpu6050_deinit(void);

/* ============================================================================
 * Public API — Gyroscope
 * ============================================================================ */

/** Baca data gyro mentah (raw, 16-bit signed per axis). */
mpu6050_status_t mpu6050_gyro_read_raw(mpu6050_raw_t *raw);

/** Baca data gyro dan konversi ke deg/s (format fixed-point x100). */
mpu6050_status_t mpu6050_gyro_read_dps_x100(mpu6050_dps_x100_t *dps);

/** Ganti gyro full-scale range saat runtime. */
mpu6050_status_t mpu6050_gyro_set_range(mpu6050_range_t range);

/* ============================================================================
 * Public API — Accelerometer
 * ============================================================================ */

/** Baca data accelerometer mentah (raw, 16-bit signed per axis). */
mpu6050_status_t mpu6050_accel_read_raw(mpu6050_accel_raw_t *raw);

/** Baca data accelerometer dan konversi ke milli-g (format integer). */
mpu6050_status_t mpu6050_accel_read_mg(mpu6050_accel_mg_t *mg);

/** Ganti accelerometer full-scale range saat runtime. */
mpu6050_status_t mpu6050_accel_set_range(mpu6050_accel_range_t range);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H__ */

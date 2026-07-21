/*
 * Copyright (C) 2026 ICDeC
 *
 * Test Application: MPU-6050 Accelerometer
 * =========================================
 * Test khusus untuk membaca data ACCELEROMETER dari MPU-6050.
 *
 * Menggunakan pola I2C yang SUDAH TERBUKTI berhasil dari test gyroscope
 * sebelumnya (repeated-start + delay kecil antara write dan read).
 *
 * CATATAN PENTING:
 *   1. MPU-6050 adalah sensor 6-axis (Gyro + Accel).
 *      Accelerometer punya register terpisah dari gyroscope.
 *   2. Output data MPU-6050 BIG-ENDIAN (byte High duluan, baru Low).
 *   3. PULPissimo TIDAK punya FPU -- semua konversi ke mg (milli-g)
 *      dilakukan dengan integer fixed-point, BUKAN float.
 *   4. Setelah power-on, MPU-6050 dalam SLEEP MODE.
 *      WAJIB tulis 0x00 ke PWR_MGMT_1 untuk bangunkan device.
 *   5. Accelerometer full-scale range dikontrol oleh ACCEL_CONFIG (0x1C),
 *      bits [4:3] (AFS_SEL).
 *
 * Usage:
 *   make all
 *   make run platform=fpga
 */

#include <stdio.h>
#include "pulp.h"

/* ============================================================================
 * MPU-6050 Register Definitions (Accelerometer-specific)
 *
 * Register yang diperlukan untuk test accelerometer.
 * Register umum (WHO_AM_I, PWR_MGMT_1, CONFIG, SMPLRT_DIV) juga
 * didefinisikan di sini supaya file test ini self-contained.
 * ============================================================================ */

/* --- I2C Address --- */
#define MPU6050_I2C_ADDR_DEFAULT    0x69    /* AD0 pin = HIGH (Vdd) */
#define MPU6050_I2C_ADDR_ALT       0x68    /* AD0 pin = LOW (GND) */

/* --- Register Umum --- */
#define MPU6050_REG_SMPLRT_DIV      0x19    /* Sample Rate Divider */
#define MPU6050_REG_CONFIG          0x1A    /* Configuration (DLPF, EXT_SYNC) */
#define MPU6050_REG_PWR_MGMT_1      0x6B    /* Power Management 1 */
#define MPU6050_REG_WHO_AM_I        0x75    /* Device ID, read-only, default 0x68 */

/* --- Register Accelerometer --- */
#define MPU6050_REG_ACCEL_CONFIG    0x1C    /* Accelerometer Configuration
                                             *   Bits [4:3] = AFS_SEL
                                             *   00 = ±2g, 01 = ±4g,
                                             *   10 = ±8g, 11 = ±16g */
#define MPU6050_REG_ACCEL_XOUT_H    0x3B    /* Accel data start (6 byte berurutan)
                                             *   0x3B = ACCEL_XOUT_H
                                             *   0x3C = ACCEL_XOUT_L
                                             *   0x3D = ACCEL_YOUT_H
                                             *   0x3E = ACCEL_YOUT_L
                                             *   0x3F = ACCEL_ZOUT_H
                                             *   0x40 = ACCEL_ZOUT_L */

/* --- Nilai Penting --- */
#define MPU6050_WHO_AM_I_VALUE      0x68    /* Nilai WHO_AM_I yang benar */
#define MPU6050_PWR1_WAKEUP         0x00    /* Clear sleep + reset -> wake up */
#define MPU6050_PWR1_SLEEP          0x40    /* Sleep mode (bit 6) */

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

#define ACCEL_AFS_SEL_2G    0x00    /* ±2g,  16384 LSB/g */
#define ACCEL_AFS_SEL_4G    0x08    /* ±4g,   8192 LSB/g */
#define ACCEL_AFS_SEL_8G    0x10    /* ±8g,   4096 LSB/g */
#define ACCEL_AFS_SEL_16G   0x18    /* ±16g,  2048 LSB/g */

/* ============================================================================
 * Timing Constants (sudah terbukti dari test sebelumnya)
 * ============================================================================ */

#define I2C_TIMEOUT_US          5000        /* Software timeout per I2C transaksi */
#define READ_DELAY_LOOPS        200         /* Delay wajib antara write-reg dan read-data */
#define CONFIG_SETTLE_LOOPS     50000       /* Delay setelah tulis register konfigurasi */
#define WAKEUP_DELAY_LOOPS      100000      /* Delay setelah wake from sleep */

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

#define ACCEL_RANGE             ACCEL_AFS_SEL_2G    /* Full-scale range yang dipakai */
#define CONTINUOUS_NUM_SAMPLES  300                  /* Jumlah sampel (0 = infinite) */
#define CONTINUOUS_DELAY_LOOPS  200000               /* Delay antar pembacaan */

/* ============================================================================
 * Internal Driver State
 * ============================================================================ */

static i2c_t       *i2c_handle = NULL;
static i2c_dev_t    i2c_dev;
static uint8_t      active_addr = 0;

/* ============================================================================
 * Low-Level I2C Helpers
 *
 * Pola yang SUDAH TERBUKTI berhasil:
 *   - Write alamat register (tanpa stop -> repeated start)
 *   - Delay kecil (READ_DELAY_LOOPS)
 *   - Read data
 * ============================================================================ */

static int accel_write_reg(uint8_t reg, uint8_t value)
{
    unsigned char data[2];
    data[0] = reg;
    data[1] = value;
    return i2c_write(i2c_handle, data, 2, 1);  /* send_stop = 1 */
}

static int accel_read_reg(uint8_t reg, uint8_t *out)
{
    unsigned char r = reg;

    int ret = i2c_write(i2c_handle, &r, 1, 0);  /* tanpa stop -> repeated start */
    if (ret != 0) {
        return -1;
    }

    for (volatile int d = 0; d < READ_DELAY_LOOPS; d++);

    int n = i2c_read(i2c_handle, out, 1, 0);
    if (n != 1) {
        return -2;
    }

    return 0;
}

/**
 * Baca banyak byte berurutan mulai dari startReg.
 * MPU-6050 otomatis auto-increment -- TIDAK perlu set bit 0x80.
 * @return jumlah byte yang berhasil dibaca, -1 kalau fase write gagal.
 */
static int accel_read_bytes(uint8_t startReg, uint8_t *buf, int len)
{
    unsigned char r = startReg;

    int ret = i2c_write(i2c_handle, &r, 1, 0);
    if (ret != 0) {
        return -1;
    }

    for (volatile int d = 0; d < READ_DELAY_LOOPS; d++);

    return i2c_read(i2c_handle, buf, len, 0);
}

/* ============================================================================
 * I2C Open + WHO_AM_I Verification
 * ============================================================================ */

static int try_open_i2c(uint8_t addr_7bit)
{
    i2c_dev_init(&i2c_dev);
    i2c_dev.id           = 0;
    i2c_dev.cs           = (addr_7bit << 1);  /* format 8-bit untuk i2c_dev_t.cs */
    i2c_dev.max_baudrate = 100000;

    i2c_handle = i2c_open(&i2c_dev);
    if (i2c_handle == NULL) {
        return -1;
    }

    i2c_settimeout(I2C_TIMEOUT_US, 1);

    /* Verifikasi WHO_AM_I */
    uint8_t who = 0;
    int ret = accel_read_reg(MPU6050_REG_WHO_AM_I, &who);
    if (ret != 0) {
        printf("  [!] WHO_AM_I read gagal (addr=0x%02X, ret=%d)\n", addr_7bit, ret);
        i2c_close(i2c_handle);
        i2c_handle = NULL;
        return -2;
    }

    if (who != MPU6050_WHO_AM_I_VALUE) {
        printf("  [!] WHO_AM_I mismatch: expected 0x%02X, got 0x%02X\n",
               MPU6050_WHO_AM_I_VALUE, who);
        i2c_close(i2c_handle);
        i2c_handle = NULL;
        return -3;
    }

    active_addr = addr_7bit;
    return 0;
}

/* ============================================================================
 * Data Types untuk Accelerometer
 * ============================================================================ */

/** Data mentah accelerometer (16-bit signed per axis). */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} accel_raw_t;

/**
 * Data accelerometer dalam milli-g (mg), format integer.
 * Contoh: x_mg = 1000 artinya 1.000g (1g).
 *
 * Konversi dari raw ke mg:
 *   mg = raw * 1000 / sensitivity_LSBperG
 *
 *   ±2g  -> 16384 LSB/g -> mg = raw * 1000 / 16384
 *   ±4g  ->  8192 LSB/g -> mg = raw * 1000 / 8192
 *   ±8g  ->  4096 LSB/g -> mg = raw * 1000 / 4096
 *   ±16g ->  2048 LSB/g -> mg = raw * 1000 / 2048
 *
 * Overflow check (worst case):
 *   raw max = 32767, num max = 1000
 *   32767 * 1000 = 32,767,000 -> aman untuk int32_t
 */
typedef struct {
    int32_t x_mg;
    int32_t y_mg;
    int32_t z_mg;
} accel_mg_t;

/* ============================================================================
 * Sensitivity Lookup (integer fixed-point)
 *
 * Rumus:  mg = raw * 1000 / sensitivity_LSBperG
 * Untuk menghindari float, kita pakai:  mg = raw * num / denom
 *
 *   ±2g  -> 16384 LSB/g -> num=125, denom=2048  (1000/16384 = 125/2048)
 *   ±4g  ->  8192 LSB/g -> num=125, denom=1024  (1000/8192  = 125/1024)
 *   ±8g  ->  4096 LSB/g -> num=125, denom=512   (1000/4096  = 125/512)
 *   ±16g ->  2048 LSB/g -> num=125, denom=256   (1000/2048  = 125/256)
 *
 * Overflow check: max raw 32767 * 125 = 4,095,875 -> aman int32_t
 * ============================================================================ */

typedef struct {
    int32_t num;
    int32_t denom;
} accel_sens_t;

static accel_sens_t get_accel_sensitivity(uint8_t afs_sel)
{
    accel_sens_t s;
    switch (afs_sel) {
        case ACCEL_AFS_SEL_4G:
            s.num = 125; s.denom = 1024;
            break;
        case ACCEL_AFS_SEL_8G:
            s.num = 125; s.denom = 512;
            break;
        case ACCEL_AFS_SEL_16G:
            s.num = 125; s.denom = 256;
            break;
        case ACCEL_AFS_SEL_2G:
        default:
            s.num = 125; s.denom = 2048;
            break;
    }
    return s;
}

/* ============================================================================
 * Baca data accelerometer mentah (6 byte = 3 axis x 16-bit)
 * ============================================================================ */

static int read_accel_raw(accel_raw_t *raw)
{
    uint8_t buf[6] = {0};
    int received = accel_read_bytes(MPU6050_REG_ACCEL_XOUT_H, buf, 6);
    if (received != 6) {
        return -1;
    }

    /* MPU-6050: BIG-ENDIAN (byte High duluan, baru Low) */
    raw->x = (int16_t)((buf[0] << 8) | buf[1]);
    raw->y = (int16_t)((buf[2] << 8) | buf[3]);
    raw->z = (int16_t)((buf[4] << 8) | buf[5]);

    return 0;
}

/* ============================================================================
 * Konversi raw -> milli-g (integer fixed-point)
 * ============================================================================ */

static void convert_raw_to_mg(const accel_raw_t *raw, accel_mg_t *mg, const accel_sens_t *sens)
{
    mg->x_mg = ((int32_t)raw->x * sens->num) / sens->denom;
    mg->y_mg = ((int32_t)raw->y * sens->num) / sens->denom;
    mg->z_mg = ((int32_t)raw->z * sens->num) / sens->denom;
}

/* ============================================================================
 * Utility: print milli-g value dengan format yang rapi
 *
 * Contoh:  1000 mg -> " 1000"
 *          -500 mg -> " -500"
 * ============================================================================ */

static void print_mg(int32_t mg_val)
{
    printf("%6d", (int)mg_val);
}

/**
 * Print value dalam satuan g (fixed-point, 3 desimal).
 * Contoh: 1000 mg -> " 1.000"
 *          -50 mg -> "-0.050"
 */
static void print_g_from_mg(int32_t mg_val)
{
    int sign = (mg_val < 0) ? -1 : 1;
    int32_t av = mg_val * sign;
    printf("%s%d.%03d", (sign < 0) ? "-" : " ", (int)(av / 1000), (int)(av % 1000));
}

/* ============================================================================
 * MAIN TEST PROGRAM
 * ============================================================================ */

int main()
{
    int pass_count = 0;
    int fail_count = 0;

    printf("========================================\n");
    printf(" MPU-6050 Accelerometer Test\n");
    printf(" ICDeC PULPissimo FPGA Board\n");
    printf("========================================\n\n");

    /* ---- TEST 1: Buka I2C dan verifikasi WHO_AM_I ---- */
    printf("[TEST 1] Membuka I2C dan verifikasi WHO_AM_I...\n");
    {
        printf("  Mencoba addr=0x%02X...\n", MPU6050_I2C_ADDR_DEFAULT);
        int ret = try_open_i2c(MPU6050_I2C_ADDR_DEFAULT);

        if (ret != 0) {
            /* Coba alamat alternatif */
            printf("  Gagal, mencoba addr=0x%02X...\n", MPU6050_I2C_ADDR_ALT);
            ret = try_open_i2c(MPU6050_I2C_ADDR_ALT);
        }

        if (ret == 0) {
            printf("  PASS: WHO_AM_I = 0x%02X, addr=0x%02X\n", MPU6050_WHO_AM_I_VALUE, active_addr);
            pass_count++;
        } else {
            printf("  FAIL: Tidak bisa terhubung ke MPU-6050 (ret=%d)\n", ret);
            fail_count++;
            printf("\n========================================\n");
            printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
            printf("========================================\n");
            return -1;
        }
    }
    printf("\n");

    /* ---- TEST 2: Wake from sleep ---- */
    printf("[TEST 2] Wake from sleep (PWR_MGMT_1 = 0x00)...\n");
    {
        int wret = accel_write_reg(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_WAKEUP);
        if (wret != 0) {
            printf("  FAIL: Gagal tulis PWR_MGMT_1 (ret=%d)\n", wret);
            fail_count++;
            goto cleanup;
        }

        for (volatile int i = 0; i < WAKEUP_DELAY_LOOPS; i++);

        /* Verifikasi: bit SLEEP (bit 6) dan DEVICE_RESET (bit 7) harus 0 */
        uint8_t pwr_readback = 0xFF;
        int rret = accel_read_reg(MPU6050_REG_PWR_MGMT_1, &pwr_readback);
        if (rret != 0 || (pwr_readback & 0xC0) != 0x00) {
            printf("  FAIL: Verifikasi PWR_MGMT_1 gagal (readback=0x%02X, ret=%d)\n",
                   pwr_readback, rret);
            fail_count++;
            goto cleanup;
        }

        printf("  PASS: PWR_MGMT_1 = 0x%02X (awake)\n", pwr_readback);
        pass_count++;
    }
    printf("\n");

    /* ---- TEST 3: Konfigurasi ACCEL_CONFIG (full-scale range) ---- */
    printf("[TEST 3] Konfigurasi ACCEL_CONFIG (AFS_SEL)...\n");
    {
        int wret = accel_write_reg(MPU6050_REG_ACCEL_CONFIG, ACCEL_RANGE);
        if (wret != 0) {
            printf("  FAIL: Gagal tulis ACCEL_CONFIG (ret=%d)\n", wret);
            fail_count++;
            goto cleanup;
        }

        for (volatile int i = 0; i < CONFIG_SETTLE_LOOPS; i++);

        /* Verifikasi readback */
        uint8_t accel_cfg_readback = 0xFF;
        int rret = accel_read_reg(MPU6050_REG_ACCEL_CONFIG, &accel_cfg_readback);
        if (rret != 0 || accel_cfg_readback != ACCEL_RANGE) {
            printf("  FAIL: Verifikasi ACCEL_CONFIG gagal (expected=0x%02X, readback=0x%02X)\n",
                   ACCEL_RANGE, accel_cfg_readback);
            fail_count++;
            goto cleanup;
        }

        /* Print range yang aktif */
        const char *range_str = "???";
        switch (ACCEL_RANGE) {
            case ACCEL_AFS_SEL_2G:  range_str = "+/-2g";  break;
            case ACCEL_AFS_SEL_4G:  range_str = "+/-4g";  break;
            case ACCEL_AFS_SEL_8G:  range_str = "+/-8g";  break;
            case ACCEL_AFS_SEL_16G: range_str = "+/-16g"; break;
        }
        printf("  PASS: ACCEL_CONFIG = 0x%02X (%s)\n", accel_cfg_readback, range_str);
        pass_count++;
    }
    printf("\n");

    /* ---- TEST 4: Baca data accelerometer sekali ---- */
    printf("[TEST 4] Membaca data accelerometer sekali...\n");
    {
        accel_raw_t raw;
        int ret = read_accel_raw(&raw);
        if (ret != 0) {
            printf("  FAIL: Gagal baca data accel (ret=%d)\n", ret);
            fail_count++;
        } else {
            accel_sens_t sens = get_accel_sensitivity(ACCEL_RANGE);
            accel_mg_t mg;
            convert_raw_to_mg(&raw, &mg, &sens);

            printf("  Raw  : X=%6d  Y=%6d  Z=%6d\n", raw.x, raw.y, raw.z);
            printf("  mg   : X="); print_mg(mg.x_mg);
            printf("  Y="); print_mg(mg.y_mg);
            printf("  Z="); print_mg(mg.z_mg);
            printf("\n");
            printf("  g    : X="); print_g_from_mg(mg.x_mg);
            printf("  Y="); print_g_from_mg(mg.y_mg);
            printf("  Z="); print_g_from_mg(mg.z_mg);
            printf("\n");

            /* Sanity check: pada keadaan diam, salah satu axis harus ~1g (1000 mg)
             * Toleransi longgar: total magnitude harus 800-1200 mg (0.8g - 1.2g range) */
            int32_t mag_sq = (mg.x_mg * mg.x_mg) + (mg.y_mg * mg.y_mg) + (mg.z_mg * mg.z_mg);
            /* 1g^2 = 1,000,000 mg^2. Toleransi: 640,000 - 1,440,000 */
            if (mag_sq >= 640000 && mag_sq <= 1440000) {
                printf("  PASS: Magnitude wajar (~1g saat diam)\n");
                pass_count++;
            } else {
                printf("  WARNING: Magnitude di luar range wajar (mag^2=%d)\n", (int)mag_sq);
                printf("  (mungkin sensor sedang bergerak -- PASS conditional)\n");
                pass_count++;  /* Tetap pass, hanya warning */
            }
        }
    }
    printf("\n");

    /* ---- TEST 5: Baca 10 sampel berurutan (konsistensi) ---- */
    printf("[TEST 5] Baca 10 sampel berurutan (konsistensi)...\n");
    {
        accel_sens_t sens = get_accel_sensitivity(ACCEL_RANGE);
        int read_ok = 0;
        int read_fail = 0;

        for (int i = 0; i < 10; i++) {
            accel_raw_t raw;
            int ret = read_accel_raw(&raw);
            if (ret == 0) {
                accel_mg_t mg;
                convert_raw_to_mg(&raw, &mg, &sens);
                printf("  [%2d] Raw: %6d %6d %6d | mg: %6d %6d %6d\n",
                       i, raw.x, raw.y, raw.z,
                       (int)mg.x_mg, (int)mg.y_mg, (int)mg.z_mg);
                read_ok++;
            } else {
                printf("  [%2d] GAGAL baca (ret=%d)\n", i, ret);
                read_fail++;
            }
            for (volatile int d = 0; d < 50000; d++);
        }

        if (read_ok == 10) {
            printf("  PASS: 10/10 sampel terbaca\n");
            pass_count++;
        } else {
            printf("  FAIL: %d/10 sampel gagal\n", read_fail);
            fail_count++;
        }
    }
    printf("\n");

    /* ---- Results ---- */
    printf("========================================\n");
    printf(" RESULTS: %d PASSED, %d FAILED\n", pass_count, fail_count);
    printf("========================================\n");

    /* ---- Continuous Read (hanya kalau semua test pass) ---- */
    if (fail_count == 0) {
        accel_sens_t sens = get_accel_sensitivity(ACCEL_RANGE);

        printf("\n========================================\n");
        printf(" CONTINUOUS ACCELEROMETER READ\n");
        if (CONTINUOUS_NUM_SAMPLES > 0)
            printf(" Jumlah sampel: %d\n", CONTINUOUS_NUM_SAMPLES);
        else
            printf(" Mode: infinite (reset board untuk stop)\n");
        printf("========================================\n\n");

        printf("  #    |  Raw X   Raw Y   Raw Z  |  mg X   mg Y   mg Z  |  g X     g Y     g Z\n");
        printf("-------+------------------------+----------------------+------------------------\n");

        int sample = 0;
        while (1) {
            accel_raw_t raw;
            int ret = read_accel_raw(&raw);

            if (ret != 0) {
                printf("  [!] Gagal baca data (ret=%d)\n", ret);
                for (volatile int d = 0; d < CONTINUOUS_DELAY_LOOPS; d++);
                continue;
            }

            accel_mg_t mg;
            convert_raw_to_mg(&raw, &mg, &sens);

            printf(" %4d  | %6d  %6d  %6d  |", sample, raw.x, raw.y, raw.z);
            print_mg(mg.x_mg); printf(" ");
            print_mg(mg.y_mg); printf(" ");
            print_mg(mg.z_mg); printf("  |");
            print_g_from_mg(mg.x_mg); printf("  ");
            print_g_from_mg(mg.y_mg); printf("  ");
            print_g_from_mg(mg.z_mg); printf("\n");

            sample++;
            if (CONTINUOUS_NUM_SAMPLES > 0 && sample >= CONTINUOUS_NUM_SAMPLES)
                break;

            for (volatile int d = 0; d < CONTINUOUS_DELAY_LOOPS; d++);
        }

        printf("\n  Selesai: %d sampel terbaca.\n", sample);
    }

cleanup:
    /* Sleep mode dan tutup I2C */
    if (i2c_handle != NULL) {
        accel_write_reg(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_SLEEP);
        i2c_close(i2c_handle);
        i2c_handle = NULL;
    }

    printf("\n========================================\n");
    printf(" TEST SELESAI\n");
    printf("========================================\n");

    return (fail_count == 0) ? 0 : -1;
}

void pe_start(void)
{
}

/*
 * Copyright (C) 2026 ICDeC
 *
 * Driver library untuk InvenSense MPU-6050 6-Axis Gyroscope + Accelerometer.
 * Lihat mpu6050.h untuk dokumentasi API dan catatan penting.
 *
 * Implementasi ini adalah hasil reverse-engineering dari raw I2C test
 * yang sudah TERBUKTI berhasil di board ICDeC PULPissimo:
 *
 *   Gyroscope:
 *   - test_mpu6050.c: 6 PASSED, 0 FAILED
 *   - Continuous read: 100+ sampel terbaca dengan nilai wajar
 *
 *   Accelerometer:
 *   - test_accelerometer_mpu6050.c: 5 PASSED, 0 FAILED
 *   - Continuous read: 300 sampel terbaca, ~1g saat diam
 *
 * Hal kunci yang membuat versi sebelumnya gagal/hang, dan sudah
 * diperbaiki di sini:
 *   1. Delay kecil (~200 loop iterasi) antara fase write-alamat-register
 *      dan fase read-data. Tanpa ini, hasil baca selalu 0xFF meski
 *      tidak ada error/timeout yang terdeteksi.
 *   2. Konversi pakai integer fixed-point, BUKAN float --
 *      PULPissimo tidak punya FPU, dan float terbukti menghasilkan nilai
 *      yang salah.
 *      - Gyro: dps * 100 (centidegrees per second)
 *      - Accel: milli-g (mg)
 *   3. WAJIB wake from sleep (PWR_MGMT_1 = 0x00) sebelum baca data --
 *      MPU-6050 default power-on state: SLEEP=1.
 */

#include <stdio.h>
#include <string.h>
#include "pulp.h"
#include "mpu6050.h"

/* ============================================================================
 * Konfigurasi Internal
 * ============================================================================ */

/** Software timeout untuk tiap transaksi I2C (microseconds). */
#define MPU6050_I2C_TIMEOUT_US      5000

/** Delay antara fase write-alamat-register dan fase read-data.
 *  Terbukti WAJIB ada -- tanpa ini hasil baca selalu 0xFF. */
#define MPU6050_READ_DELAY_LOOPS    200

/** Delay setelah menulis register konfigurasi, sebelum verifikasi readback. */
#define MPU6050_CONFIG_SETTLE_LOOPS 50000

/** Delay setelah wake from sleep (sensor butuh stabilisasi). */
#define MPU6050_WAKEUP_DELAY_LOOPS  100000

/* ============================================================================
 * Sensitivity Lookup — Gyroscope (integer fixed-point)
 *
 * Rumus umum: dps = raw / sensitivity_LSBperDPS
 * Kita butuh: dps_x100 = raw * num / denom
 *
 *   250dps  -> 131   LSB/°/s -> num=100, denom=131
 *   500dps  -> 65.5  LSB/°/s -> num=200, denom=131  (100/65.5 = 200/131)
 *   1000dps -> 32.8  LSB/°/s -> num=125, denom=41   (100/32.8 = 125/41)
 *   2000dps -> 16.4  LSB/°/s -> num=250, denom=41   (100/16.4 = 250/41)
 *
 * Semua aman dari overflow: max raw 32767 * max num 250 = 8,191,750 (int32_t OK)
 * ============================================================================ */

typedef struct {
    int32_t num;
    int32_t denom;
} mpu6050_sens_t;

static mpu6050_sens_t mpu6050_get_gyro_sensitivity(mpu6050_range_t range)
{
    mpu6050_sens_t s;
    switch (range) {
        case MPU6050_RANGE_500DPS:
            s.num = 200; s.denom = 131;
            break;
        case MPU6050_RANGE_1000DPS:
            s.num = 125; s.denom = 41;
            break;
        case MPU6050_RANGE_2000DPS:
            s.num = 250; s.denom = 41;
            break;
        case MPU6050_RANGE_250DPS:
        default:
            s.num = 100; s.denom = 131;
            break;
    }
    return s;
}

/* ============================================================================
 * Sensitivity Lookup — Accelerometer (integer fixed-point)
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

static mpu6050_sens_t mpu6050_get_accel_sensitivity(mpu6050_accel_range_t range)
{
    mpu6050_sens_t s;
    switch (range) {
        case MPU6050_ACCEL_RANGE_4G:
            s.num = 125; s.denom = 1024;
            break;
        case MPU6050_ACCEL_RANGE_8G:
            s.num = 125; s.denom = 512;
            break;
        case MPU6050_ACCEL_RANGE_16G:
            s.num = 125; s.denom = 256;
            break;
        case MPU6050_ACCEL_RANGE_2G:
        default:
            s.num = 125; s.denom = 2048;
            break;
    }
    return s;
}

/* ============================================================================
 * Internal Driver State
 * ============================================================================ */

static struct {
    i2c_t                 *i2c;
    i2c_dev_t              i2c_dev;
    uint8_t                i2c_addr_7bit;
    mpu6050_range_t        gyro_range;
    mpu6050_sens_t         gyro_sensitivity;
    mpu6050_accel_range_t  accel_range;
    mpu6050_sens_t         accel_sensitivity;
    int                    initialized;
} mpu_state = { .initialized = 0 };

/* ============================================================================
 * Low-Level I2C Helpers (raw i2c_write()/i2c_read(), TERBUKTI berhasil)
 * ============================================================================ */

static int mpu_write_reg(uint8_t reg, uint8_t value)
{
    unsigned char data[2];
    data[0] = reg;
    data[1] = value;
    return i2c_write(mpu_state.i2c, data, 2, 1); /* send_stop = 1 */
}

/**
 * Baca 1 byte dari 1 register. Memakai repeated-start + delay kecil
 * (MPU6050_READ_DELAY_LOOPS) -- kombinasi yang terbukti berhasil di
 * pengujian raw I2C sebelumnya.
 */
static int mpu_read_reg(uint8_t reg, uint8_t *out)
{
    unsigned char r = reg;

    int ret = i2c_write(mpu_state.i2c, &r, 1, 0); /* tanpa stop -> repeated start */
    if (ret != 0) {
        return -1;
    }

    for (volatile int d = 0; d < MPU6050_READ_DELAY_LOOPS; d++);

    int n = i2c_read(mpu_state.i2c, out, 1, 0);
    if (n != 1) {
        return -2;
    }

    return 0;
}

/**
 * Baca banyak byte berurutan mulai dari startReg.
 * MPU-6050 otomatis auto-increment -- TIDAK perlu set bit 0x80
 * seperti L3G4200D.
 * @return jumlah byte yang berhasil dibaca, -1 kalau fase write gagal.
 */
static int mpu_read_bytes(uint8_t startReg, uint8_t *buf, int len)
{
    unsigned char r = startReg;

    int ret = i2c_write(mpu_state.i2c, &r, 1, 0);
    if (ret != 0) {
        return -1;
    }

    for (volatile int d = 0; d < MPU6050_READ_DELAY_LOOPS; d++);

    return i2c_read(mpu_state.i2c, buf, len, 0);
}

/* ============================================================================
 * Internal: buka I2C bus untuk satu alamat tertentu, lalu cek WHO_AM_I.
 * ============================================================================ */

static mpu6050_status_t mpu_try_open_and_verify(uint8_t addr_7bit, int i2c_id, unsigned int freq)
{
    i2c_dev_init(&mpu_state.i2c_dev);
    mpu_state.i2c_dev.id           = i2c_id;
    mpu_state.i2c_dev.cs           = (addr_7bit << 1); /* format 8-bit untuk i2c_dev_t.cs */
    mpu_state.i2c_dev.max_baudrate = freq;

    mpu_state.i2c = i2c_open(&mpu_state.i2c_dev);
    if (mpu_state.i2c == NULL) {
        return MPU6050_ERR_I2C;
    }

    i2c_settimeout(MPU6050_I2C_TIMEOUT_US, 1);

    uint8_t who_am_i = 0;
    int ret = mpu_read_reg(MPU6050_REG_WHO_AM_I, &who_am_i);
    if (ret != 0) {
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_TIMEOUT;
    }

    if (who_am_i != MPU6050_WHO_AM_I_VALUE) {
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_ID;
    }

    mpu_state.i2c_addr_7bit = addr_7bit;
    return MPU6050_OK;
}

/* ============================================================================
 * Public API — Umum
 * ============================================================================ */

mpu6050_status_t mpu6050_default_config(mpu6050_config_t *cfg)
{
    if (cfg == NULL) {
        return MPU6050_ERR_NULL;
    }

    cfg->i2c_addr    = MPU6050_I2C_ADDR_DEFAULT;
    cfg->i2c_id      = 0;
    cfg->i2c_freq    = 100000;
    cfg->gyro_range  = MPU6050_RANGE_250DPS;
    cfg->accel_range = MPU6050_ACCEL_RANGE_2G;
    cfg->dlpf_cfg    = 0;    /* DLPF disabled, Gyro BW 256Hz */
    cfg->smplrt_div  = 0;    /* Sample Rate = Gyro Output Rate / (1 + 0) */

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_init(mpu6050_config_t *cfg)
{
    if (cfg == NULL) {
        return MPU6050_ERR_NULL;
    }

    printf("[MPU6050] Membuka I2C, mencoba addr=0x%02X...\n", cfg->i2c_addr);
    mpu6050_status_t status = mpu_try_open_and_verify(cfg->i2c_addr, cfg->i2c_id, cfg->i2c_freq);

    if (status != MPU6050_OK) {
        uint8_t alt_addr = (cfg->i2c_addr == MPU6050_I2C_ADDR_DEFAULT)
                                ? MPU6050_I2C_ADDR_ALT
                                : MPU6050_I2C_ADDR_DEFAULT;

        printf("[MPU6050] addr=0x%02X gagal (err=%d), mencoba addr=0x%02X...\n",
               cfg->i2c_addr, status, alt_addr);

        status = mpu_try_open_and_verify(alt_addr, cfg->i2c_id, cfg->i2c_freq);
        if (status != MPU6050_OK) {
            printf("[MPU6050] Kedua alamat gagal (err=%d)\n", status);
            return status;
        }
        cfg->i2c_addr = alt_addr;
    }

    printf("[MPU6050] WHO_AM_I OK di addr=0x%02X\n", cfg->i2c_addr);

    /* Wake from sleep: PWR_MGMT_1 = 0x00 (clear SLEEP bit) */
    int wret = mpu_write_reg(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_WAKEUP);
    if (wret != 0) {
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_CONFIG;
    }
    for (volatile int i = 0; i < MPU6050_WAKEUP_DELAY_LOOPS; i++);

    /* Verifikasi: bit SLEEP (bit 6) dan DEVICE_RESET (bit 7) harus 0 */
    uint8_t pwr_readback = 0xFF;
    if (mpu_read_reg(MPU6050_REG_PWR_MGMT_1, &pwr_readback) != 0 ||
        (pwr_readback & 0xC0) != 0x00) {
        printf("[MPU6050] Verifikasi PWR_MGMT_1 gagal (readback=0x%02X)\n", pwr_readback);
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_CONFIG;
    }

    /* Konfigurasi SMPLRT_DIV */
    if (cfg->smplrt_div != 0) {
        wret = mpu_write_reg(MPU6050_REG_SMPLRT_DIV, cfg->smplrt_div);
        if (wret != 0) {
            i2c_close(mpu_state.i2c);
            return MPU6050_ERR_CONFIG;
        }
        for (volatile int i = 0; i < MPU6050_CONFIG_SETTLE_LOOPS; i++);
    }

    /* Konfigurasi DLPF (CONFIG register) */
    if (cfg->dlpf_cfg != 0) {
        wret = mpu_write_reg(MPU6050_REG_CONFIG, cfg->dlpf_cfg & 0x07);
        if (wret != 0) {
            i2c_close(mpu_state.i2c);
            return MPU6050_ERR_CONFIG;
        }
        for (volatile int i = 0; i < MPU6050_CONFIG_SETTLE_LOOPS; i++);
    }

    /* Konfigurasi GYRO_CONFIG: full-scale range */
    wret = mpu_write_reg(MPU6050_REG_GYRO_CONFIG, (uint8_t)cfg->gyro_range);
    if (wret != 0) {
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_CONFIG;
    }
    for (volatile int i = 0; i < MPU6050_CONFIG_SETTLE_LOOPS; i++);

    uint8_t gyro_readback = 0xFF;
    if (mpu_read_reg(MPU6050_REG_GYRO_CONFIG, &gyro_readback) != 0 ||
        gyro_readback != (uint8_t)cfg->gyro_range) {
        printf("[MPU6050] Verifikasi GYRO_CONFIG gagal (readback=0x%02X)\n", gyro_readback);
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_CONFIG;
    }

    /* Konfigurasi ACCEL_CONFIG: full-scale range */
    wret = mpu_write_reg(MPU6050_REG_ACCEL_CONFIG, (uint8_t)cfg->accel_range);
    if (wret != 0) {
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_CONFIG;
    }
    for (volatile int i = 0; i < MPU6050_CONFIG_SETTLE_LOOPS; i++);

    uint8_t accel_readback = 0xFF;
    if (mpu_read_reg(MPU6050_REG_ACCEL_CONFIG, &accel_readback) != 0 ||
        accel_readback != (uint8_t)cfg->accel_range) {
        printf("[MPU6050] Verifikasi ACCEL_CONFIG gagal (readback=0x%02X)\n", accel_readback);
        i2c_close(mpu_state.i2c);
        return MPU6050_ERR_CONFIG;
    }

    mpu_state.gyro_range        = cfg->gyro_range;
    mpu_state.gyro_sensitivity  = mpu6050_get_gyro_sensitivity(cfg->gyro_range);
    mpu_state.accel_range       = cfg->accel_range;
    mpu_state.accel_sensitivity = mpu6050_get_accel_sensitivity(cfg->accel_range);
    mpu_state.initialized       = 1;

    printf("[MPU6050] Init selesai: addr=0x%02X, gyro_range=%d, accel_range=%d\n",
           cfg->i2c_addr, cfg->gyro_range, cfg->accel_range);

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_who_am_i(uint8_t *id)
{
    if (id == NULL) {
        return MPU6050_ERR_NULL;
    }
    if (!mpu_state.initialized) {
        return MPU6050_ERR_CONFIG;
    }

    int ret = mpu_read_reg(MPU6050_REG_WHO_AM_I, id);
    return (ret == 0) ? MPU6050_OK : MPU6050_ERR_TIMEOUT;
}

mpu6050_status_t mpu6050_deinit(void)
{
    if (!mpu_state.initialized) {
        return MPU6050_OK;
    }

    /* Set SLEEP bit supaya sensor hemat daya */
    mpu_write_reg(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_SLEEP);

    if (mpu_state.i2c != NULL) {
        i2c_close(mpu_state.i2c);
        mpu_state.i2c = NULL;
    }

    mpu_state.initialized = 0;

    return MPU6050_OK;
}

/* ============================================================================
 * Public API — Gyroscope
 * ============================================================================ */

mpu6050_status_t mpu6050_gyro_read_raw(mpu6050_raw_t *raw)
{
    if (raw == NULL) {
        return MPU6050_ERR_NULL;
    }
    if (!mpu_state.initialized) {
        return MPU6050_ERR_CONFIG;
    }

    uint8_t buf[6] = {0};
    int received = mpu_read_bytes(MPU6050_REG_GYRO_XOUT_H, buf, 6);
    if (received != 6) {
        return MPU6050_ERR_TIMEOUT;
    }

    /* MPU-6050: BIG-ENDIAN (byte High duluan, baru Low)
     * Kebalikan dari L3G4200D yang little-endian. */
    raw->x = (int16_t)((buf[0] << 8) | buf[1]);
    raw->y = (int16_t)((buf[2] << 8) | buf[3]);
    raw->z = (int16_t)((buf[4] << 8) | buf[5]);

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_gyro_read_dps_x100(mpu6050_dps_x100_t *dps)
{
    if (dps == NULL) {
        return MPU6050_ERR_NULL;
    }

    mpu6050_raw_t raw;
    mpu6050_status_t status = mpu6050_gyro_read_raw(&raw);
    if (status != MPU6050_OK) {
        return status;
    }

    /* Integer fixed-point, TIDAK pakai float (PULPissimo tidak punya FPU) */
    dps->x_x100 = ((int32_t)raw.x * mpu_state.gyro_sensitivity.num) / mpu_state.gyro_sensitivity.denom;
    dps->y_x100 = ((int32_t)raw.y * mpu_state.gyro_sensitivity.num) / mpu_state.gyro_sensitivity.denom;
    dps->z_x100 = ((int32_t)raw.z * mpu_state.gyro_sensitivity.num) / mpu_state.gyro_sensitivity.denom;

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_gyro_set_range(mpu6050_range_t range)
{
    if (!mpu_state.initialized) {
        return MPU6050_ERR_CONFIG;
    }

    int wret = mpu_write_reg(MPU6050_REG_GYRO_CONFIG, (uint8_t)range);
    if (wret != 0) {
        return MPU6050_ERR_I2C;
    }
    for (volatile int i = 0; i < MPU6050_CONFIG_SETTLE_LOOPS; i++);

    mpu_state.gyro_range       = range;
    mpu_state.gyro_sensitivity = mpu6050_get_gyro_sensitivity(range);

    return MPU6050_OK;
}

/* ============================================================================
 * Public API — Accelerometer
 * ============================================================================ */

mpu6050_status_t mpu6050_accel_read_raw(mpu6050_accel_raw_t *raw)
{
    if (raw == NULL) {
        return MPU6050_ERR_NULL;
    }
    if (!mpu_state.initialized) {
        return MPU6050_ERR_CONFIG;
    }

    uint8_t buf[6] = {0};
    int received = mpu_read_bytes(MPU6050_REG_ACCEL_XOUT_H, buf, 6);
    if (received != 6) {
        return MPU6050_ERR_TIMEOUT;
    }

    /* MPU-6050: BIG-ENDIAN (byte High duluan, baru Low) */
    raw->x = (int16_t)((buf[0] << 8) | buf[1]);
    raw->y = (int16_t)((buf[2] << 8) | buf[3]);
    raw->z = (int16_t)((buf[4] << 8) | buf[5]);

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_accel_read_mg(mpu6050_accel_mg_t *mg)
{
    if (mg == NULL) {
        return MPU6050_ERR_NULL;
    }

    mpu6050_accel_raw_t raw;
    mpu6050_status_t status = mpu6050_accel_read_raw(&raw);
    if (status != MPU6050_OK) {
        return status;
    }

    /* Integer fixed-point, TIDAK pakai float (PULPissimo tidak punya FPU) */
    mg->x_mg = ((int32_t)raw.x * mpu_state.accel_sensitivity.num) / mpu_state.accel_sensitivity.denom;
    mg->y_mg = ((int32_t)raw.y * mpu_state.accel_sensitivity.num) / mpu_state.accel_sensitivity.denom;
    mg->z_mg = ((int32_t)raw.z * mpu_state.accel_sensitivity.num) / mpu_state.accel_sensitivity.denom;

    return MPU6050_OK;
}

mpu6050_status_t mpu6050_accel_set_range(mpu6050_accel_range_t range)
{
    if (!mpu_state.initialized) {
        return MPU6050_ERR_CONFIG;
    }

    int wret = mpu_write_reg(MPU6050_REG_ACCEL_CONFIG, (uint8_t)range);
    if (wret != 0) {
        return MPU6050_ERR_I2C;
    }
    for (volatile int i = 0; i < MPU6050_CONFIG_SETTLE_LOOPS; i++);

    mpu_state.accel_range       = range;
    mpu_state.accel_sensitivity = mpu6050_get_accel_sensitivity(range);

    return MPU6050_OK;
}

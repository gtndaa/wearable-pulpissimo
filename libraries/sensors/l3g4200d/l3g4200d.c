/*
 * Copyright (C) 2026 ICDeC
 *
 * Driver library minimal untuk STMicroelectronics L3G4200D 3-Axis Gyroscope.
 * Lihat l3g4200d.h untuk dokumentasi API dan catatan penting.
 *
 * Implementasi ini adalah hasil reverse-engineering dari raw I2C test
 * (test_l3g4200d.c versi "Raw I2C Test") yang sudah TERBUKTI berhasil:
 *   - TEST 0-5: 6 PASSED, 0 FAILED
 *   - Continuous read: 100 sampel terbaca dengan nilai wajar
 *
 * Dua hal kunci yang membuat versi sebelumnya (l3g4200d_init() lama)
 * gagal/hang, dan sudah diperbaiki di sini:
 *   1. Delay kecil (~200 loop iterasi) antara fase write-alamat-register
 *      dan fase read-data. Tanpa ini, hasil baca selalu 0xFF meski
 *      tidak ada error/timeout yang terdeteksi oleh i2c_write()/i2c_read().
 *   2. Konversi ke deg/s pakai integer fixed-point (x100), BUKAN float --
 *      PULPissimo tidak punya FPU, dan float terbukti menghasilkan nilai
 *      yang salah.
 */

#include <stdio.h>
#include <string.h>
#include "pulp.h"
#include "l3g4200d.h"

/* ============================================================================
 * Konfigurasi Internal
 * ============================================================================ */

/** Software timeout untuk tiap transaksi I2C (microseconds).
 *  Ini memakai mekanisme timeout BAWAAN i2c.c (pos_tick_get_counter_us()),
 *  bukan guard timer terpisah. */
#define L3G4200D_I2C_TIMEOUT_US   5000

/** Delay antara fase write-alamat-register dan fase read-data.
 *  Terbukti WAJIB ada -- tanpa ini hasil baca selalu 0xFF. */
#define L3G4200D_READ_DELAY_LOOPS 200

/** Delay setelah menulis register konfigurasi, sebelum verifikasi readback. */
#define L3G4200D_CONFIG_SETTLE_LOOPS 50000

/* ============================================================================
 * Sensitivity Lookup (fixed-point, mdps/digit * 100)
 *   250dps  -> 8.75  mdps/digit -> num=875,  denom=1000
 *   500dps  -> 17.50 mdps/digit -> num=1750, denom=1000
 *   2000dps -> 70.00 mdps/digit -> num=7000, denom=1000
 * ============================================================================ */

typedef struct {
    int32_t num;
    int32_t denom;
} l3g4200d_sens_t;

static l3g4200d_sens_t l3g4200d_get_sensitivity(l3g4200d_range_t range)
{
    l3g4200d_sens_t s;
    switch (range) {
        case L3G4200D_RANGE_500DPS:
            s.num = 1750; s.denom = 1000;
            break;
        case L3G4200D_RANGE_2000DPS:
            s.num = 7000; s.denom = 1000;
            break;
        case L3G4200D_RANGE_250DPS:
        default:
            s.num = 875; s.denom = 1000;
            break;
    }
    return s;
}

/* ============================================================================
 * Internal Driver State
 * ============================================================================ */

static struct {
    i2c_t            *i2c;
    i2c_dev_t         i2c_dev;
    uint8_t           i2c_addr_7bit;
    l3g4200d_range_t  range;
    l3g4200d_sens_t   sensitivity;
    int               initialized;
} l3g_state = { .initialized = 0 };

/* ============================================================================
 * Low-Level I2C Helpers (raw i2c_write()/i2c_read(), TERBUKTI berhasil)
 * ============================================================================ */

static int l3g_write_reg(uint8_t reg, uint8_t value)
{
    unsigned char data[2];
    data[0] = reg;
    data[1] = value;
    return i2c_write(l3g_state.i2c, data, 2, 1); /* send_stop = 1 */
}

/**
 * Baca 1 byte dari 1 register. Memakai repeated-start + delay kecil
 * (L3G4200D_READ_DELAY_LOOPS) -- kombinasi yang terbukti berhasil di
 * pengujian raw I2C sebelumnya.
 */
static int l3g_read_reg(uint8_t reg, uint8_t *out)
{
    unsigned char r = reg;

    int ret = i2c_write(l3g_state.i2c, &r, 1, 0); /* tanpa stop -> repeated start */
    if (ret != 0) {
        return -1;
    }

    for (volatile int d = 0; d < L3G4200D_READ_DELAY_LOOPS; d++);

    int n = i2c_read(l3g_state.i2c, out, 1, 0);
    if (n != 1) {
        return -2;
    }

    return 0;
}

/**
 * Baca banyak byte sekaligus dengan auto-increment (WAJIB set bit 0x80
 * pada alamat register awal).
 * @return jumlah byte yang berhasil dibaca, -1 kalau fase write gagal.
 */
static int l3g_read_bytes_auto_increment(uint8_t startReg, uint8_t *buf, int len)
{
    unsigned char r = startReg | L3G4200D_AUTO_INCREMENT_BIT;

    int ret = i2c_write(l3g_state.i2c, &r, 1, 0);
    if (ret != 0) {
        return -1;
    }

    for (volatile int d = 0; d < L3G4200D_READ_DELAY_LOOPS; d++);

    return i2c_read(l3g_state.i2c, buf, len, 0);
}

/* ============================================================================
 * Internal: buka I2C bus untuk satu alamat tertentu, lalu cek WHO_AM_I.
 * ============================================================================ */

static l3g4200d_status_t l3g_try_open_and_verify(uint8_t addr_7bit, int i2c_id, unsigned int freq)
{
    i2c_dev_init(&l3g_state.i2c_dev);
    l3g_state.i2c_dev.id           = i2c_id;
    l3g_state.i2c_dev.cs           = (addr_7bit << 1); /* format 8-bit untuk i2c_dev_t.cs */
    l3g_state.i2c_dev.max_baudrate = freq;

    l3g_state.i2c = i2c_open(&l3g_state.i2c_dev);
    if (l3g_state.i2c == NULL) {
        return L3G4200D_ERR_I2C;
    }

    i2c_settimeout(L3G4200D_I2C_TIMEOUT_US, 1);

    uint8_t who_am_i = 0;
    int ret = l3g_read_reg(L3G4200D_REG_WHO_AM_I, &who_am_i);
    if (ret != 0) {
        i2c_close(l3g_state.i2c);
        return L3G4200D_ERR_TIMEOUT;
    }

    if (who_am_i != L3G4200D_WHO_AM_I_VALUE) {
        i2c_close(l3g_state.i2c);
        return L3G4200D_ERR_ID;
    }

    l3g_state.i2c_addr_7bit = addr_7bit;
    return L3G4200D_OK;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

l3g4200d_status_t l3g4200d_default_config(l3g4200d_config_t *cfg)
{
    if (cfg == NULL) {
        return L3G4200D_ERR_NULL;
    }

    cfg->i2c_addr = L3G4200D_I2C_ADDR_DEFAULT;
    cfg->i2c_id   = 0;
    cfg->i2c_freq = 100000;
    cfg->range    = L3G4200D_RANGE_250DPS;

    return L3G4200D_OK;
}

l3g4200d_status_t l3g4200d_init(l3g4200d_config_t *cfg)
{
    if (cfg == NULL) {
        return L3G4200D_ERR_NULL;
    }

    printf("[L3G4200D] Membuka I2C, mencoba addr=0x%02X...\n", cfg->i2c_addr);
    l3g4200d_status_t status = l3g_try_open_and_verify(cfg->i2c_addr, cfg->i2c_id, cfg->i2c_freq);

    if (status != L3G4200D_OK) {
        uint8_t alt_addr = (cfg->i2c_addr == L3G4200D_I2C_ADDR_DEFAULT)
                                ? L3G4200D_I2C_ADDR_ALT
                                : L3G4200D_I2C_ADDR_DEFAULT;

        printf("[L3G4200D] addr=0x%02X gagal (err=%d), mencoba addr=0x%02X...\n",
               cfg->i2c_addr, status, alt_addr);

        status = l3g_try_open_and_verify(alt_addr, cfg->i2c_id, cfg->i2c_freq);
        if (status != L3G4200D_OK) {
            printf("[L3G4200D] Kedua alamat gagal (err=%d)\n", status);
            return status;
        }
        cfg->i2c_addr = alt_addr;
    }

    printf("[L3G4200D] WHO_AM_I OK di addr=0x%02X\n", cfg->i2c_addr);

    /* Konfigurasi CTRL_REG1: power up + enable X,Y,Z, ODR=100Hz */
    int wret = l3g_write_reg(L3G4200D_REG_CTRL_REG1, L3G4200D_CTRL_REG1_NORMAL_XYZ_100HZ);
    if (wret != 0) {
        i2c_close(l3g_state.i2c);
        return L3G4200D_ERR_CONFIG;
    }
    for (volatile int i = 0; i < L3G4200D_CONFIG_SETTLE_LOOPS; i++);

    uint8_t ctrl1_readback = 0;
    if (l3g_read_reg(L3G4200D_REG_CTRL_REG1, &ctrl1_readback) != 0 ||
        ctrl1_readback != L3G4200D_CTRL_REG1_NORMAL_XYZ_100HZ) {
        printf("[L3G4200D] Verifikasi CTRL_REG1 gagal (readback=0x%02X)\n", ctrl1_readback);
        i2c_close(l3g_state.i2c);
        return L3G4200D_ERR_CONFIG;
    }

    /* Konfigurasi CTRL_REG4: full-scale range */
    wret = l3g_write_reg(L3G4200D_REG_CTRL_REG4, (uint8_t)cfg->range);
    if (wret != 0) {
        i2c_close(l3g_state.i2c);
        return L3G4200D_ERR_CONFIG;
    }
    for (volatile int i = 0; i < L3G4200D_CONFIG_SETTLE_LOOPS; i++);

    uint8_t ctrl4_readback = 0xFF;
    if (l3g_read_reg(L3G4200D_REG_CTRL_REG4, &ctrl4_readback) != 0 ||
        ctrl4_readback != (uint8_t)cfg->range) {
        printf("[L3G4200D] Verifikasi CTRL_REG4 gagal (readback=0x%02X)\n", ctrl4_readback);
        i2c_close(l3g_state.i2c);
        return L3G4200D_ERR_CONFIG;
    }

    l3g_state.range       = cfg->range;
    l3g_state.sensitivity = l3g4200d_get_sensitivity(cfg->range);
    l3g_state.initialized = 1;

    printf("[L3G4200D] Init selesai: addr=0x%02X, range=%d\n", cfg->i2c_addr, cfg->range);

    return L3G4200D_OK;
}

l3g4200d_status_t l3g4200d_who_am_i(uint8_t *id)
{
    if (id == NULL) {
        return L3G4200D_ERR_NULL;
    }
    if (!l3g_state.initialized) {
        return L3G4200D_ERR_CONFIG;
    }

    int ret = l3g_read_reg(L3G4200D_REG_WHO_AM_I, id);
    return (ret == 0) ? L3G4200D_OK : L3G4200D_ERR_TIMEOUT;
}

l3g4200d_status_t l3g4200d_read_raw(l3g4200d_raw_t *raw)
{
    if (raw == NULL) {
        return L3G4200D_ERR_NULL;
    }
    if (!l3g_state.initialized) {
        return L3G4200D_ERR_CONFIG;
    }

    uint8_t buf[6] = {0};
    int received = l3g_read_bytes_auto_increment(L3G4200D_REG_OUT_X_L, buf, 6);
    if (received != 6) {
        return L3G4200D_ERR_TIMEOUT;
    }

    /* L3G4200D: little-endian (byte Low duluan, baru High) */
    raw->x = (int16_t)((buf[1] << 8) | buf[0]);
    raw->y = (int16_t)((buf[3] << 8) | buf[2]);
    raw->z = (int16_t)((buf[5] << 8) | buf[4]);

    return L3G4200D_OK;
}

l3g4200d_status_t l3g4200d_read_dps_x100(l3g4200d_dps_x100_t *dps)
{
    if (dps == NULL) {
        return L3G4200D_ERR_NULL;
    }

    l3g4200d_raw_t raw;
    l3g4200d_status_t status = l3g4200d_read_raw(&raw);
    if (status != L3G4200D_OK) {
        return status;
    }

    /* Integer fixed-point, TIDAK pakai float (PULPissimo tidak punya FPU) */
    dps->x_x100 = ((int32_t)raw.x * l3g_state.sensitivity.num) / l3g_state.sensitivity.denom;
    dps->y_x100 = ((int32_t)raw.y * l3g_state.sensitivity.num) / l3g_state.sensitivity.denom;
    dps->z_x100 = ((int32_t)raw.z * l3g_state.sensitivity.num) / l3g_state.sensitivity.denom;

    return L3G4200D_OK;
}

l3g4200d_status_t l3g4200d_read_dps_float(l3g4200d_dps_float_t *dps)
{
    if (dps == NULL) {
        return L3G4200D_ERR_NULL;
    }

    l3g4200d_raw_t raw;
    l3g4200d_status_t status = l3g4200d_read_raw(&raw);
    if (status != L3G4200D_OK) {
        return status;
    }

    /* Konversi pakai FLOAT -- sengaja untuk menguji FPU hardware.
     * Sensitivity: mdps/digit -> dps/digit = mdps / 1000.0f
     *   250dps  -> 8.75  mdps/digit -> 0.00875f  dps/digit
     *   500dps  -> 17.50 mdps/digit -> 0.01750f  dps/digit
     *   2000dps -> 70.00 mdps/digit -> 0.07000f  dps/digit
     */
    float sens_dps;
    switch (l3g_state.range) {
        case L3G4200D_RANGE_500DPS:
            sens_dps = 17.50f / 1000.0f;
            break;
        case L3G4200D_RANGE_2000DPS:
            sens_dps = 70.00f / 1000.0f;
            break;
        case L3G4200D_RANGE_250DPS:
        default:
            sens_dps = 8.75f / 1000.0f;
            break;
    }

    dps->x = (float)raw.x * sens_dps;
    dps->y = (float)raw.y * sens_dps;
    dps->z = (float)raw.z * sens_dps;

    return L3G4200D_OK;
}

l3g4200d_status_t l3g4200d_set_range(l3g4200d_range_t range)
{
    if (!l3g_state.initialized) {
        return L3G4200D_ERR_CONFIG;
    }

    int wret = l3g_write_reg(L3G4200D_REG_CTRL_REG4, (uint8_t)range);
    if (wret != 0) {
        return L3G4200D_ERR_I2C;
    }
    for (volatile int i = 0; i < L3G4200D_CONFIG_SETTLE_LOOPS; i++);

    l3g_state.range       = range;
    l3g_state.sensitivity = l3g4200d_get_sensitivity(range);

    return L3G4200D_OK;
}

l3g4200d_status_t l3g4200d_deinit(void)
{
    if (!l3g_state.initialized) {
        return L3G4200D_OK;
    }

    l3g_write_reg(L3G4200D_REG_CTRL_REG1, L3G4200D_CTRL_REG1_POWER_DOWN);

    if (l3g_state.i2c != NULL) {
        i2c_close(l3g_state.i2c);
        l3g_state.i2c = NULL;
    }

    l3g_state.initialized = 0;

    return L3G4200D_OK;
}
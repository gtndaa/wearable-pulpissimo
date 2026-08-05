#include <stdint.h>
#include <stdio.h>
#include "pulp.h"

#include "mpu6050.h"
#include "classifier.h"

void pe_start(void) {}

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

/** Jumlah window yang akan diklasifikasi.
 *  0 = infinite, program jalan terus-menerus. */
#define NUM_WINDOWS             0

/** Delay antar pembacaan sampel (busy-wait loop). */
#define SAMPLE_DELAY_LOOPS      200000

/** Jika 1, tampilkan raw data setiap sampel (verbose). */
#define VERBOSE_RAW             0

/** Jika 1, tampilkan detail fitur setiap window (berguna untuk tuning). */
#define SHOW_FEATURES           1

/* ============================================================================
 * Helper: nama aktivitas untuk output readable
 * ============================================================================ */

static const char *activity_name(activity_t act)
{
    switch (act) {
        case ACTIVITY_UNKNOWN: return "UNKNOWN";
        case ACTIVITY_SIT:     return "SIT    ";
        case ACTIVITY_STAND:   return "STAND  ";
        case ACTIVITY_LIE:     return "LIE    ";
        case ACTIVITY_WALK:    return "WALK   ";
        case ACTIVITY_FALL:    return "FALL   ";
        default:               return "???    ";
    }
}

static const char *mpu6050_err_name(int code)
{
    switch (code) {
        case MPU6050_OK:           return "OK";
        case MPU6050_ERR_I2C_OPEN: return "ERR_I2C_OPEN";
        case MPU6050_ERR_COMM:     return "ERR_COMM";
        case MPU6050_ERR_WHO_AM_I: return "ERR_WHO_AM_I";
        case MPU6050_ERR_CFG:      return "ERR_CFG";
        case MPU6050_ERR_READ:     return "ERR_READ";
        default:                   return "???";
    }
}

static void mg_dps_to_imu_sample(const accel_data_t *accel,
                                  const gyro_data_t *gyro,
                                  imu_sample_t *out)
{
    out->ax = (int16_t)(accel->x * ACCEL_SENS_2G / 1000);
    out->ay = (int16_t)(accel->y * ACCEL_SENS_2G / 1000);
    out->az = (int16_t)(accel->z * ACCEL_SENS_2G / 1000);

    out->gx = (int16_t)(gyro->x * GYRO_SENS_250DPS / 1000);
    out->gy = (int16_t)(gyro->y * GYRO_SENS_250DPS / 1000);
    out->gz = (int16_t)(gyro->z * GYRO_SENS_250DPS / 1000);
}

static int read_sample(i2c_t *i2c, imu_sample_t *sample)
{
    accel_data_t accel;
    gyro_data_t  gyro;

    int ret = mpu6050_read_all(i2c, &accel, &gyro);
    if (ret != MPU6050_OK) {
        printf("  [WARN] Baca sampel gagal (%s)\n\r", mpu6050_err_name(ret));
        return -1;
    }

    mg_dps_to_imu_sample(&accel, &gyro, sample);
    return 0;
}

/* ============================================================================
 * Delay sederhana
 * ============================================================================ */

static void simple_delay(int loops)
{
    volatile int i;
    for (i = 0; i < loops; i++) {
        __asm__ volatile ("nop");
    }
}

/* ============================================================================
 * Tampilkan fitur untuk debugging/tuning
 * ============================================================================ */

static void print_features(const imu_sample_t *buf, int n)
{
    accel_features_t af;
    gyro_features_t gf;

    extract_features_accel(buf, n, &af);
    extract_features_gyro(buf, n, &gf);

    printf("    [Fitur] SMA=%ld SVM_max=%lu tilt=%ld | "
           "energy=%ld zcr=%ld mean_abs=%ld\n\r",
           (long)af.sma, (unsigned long)af.svm_max, (long)af.tilt_ratio,
           (long)gf.energy, (long)gf.zcr, (long)gf.mean_abs);
}

/* ============================================================================
 * Mode: Continuous classification (real-time monitoring)
 * ============================================================================ */

static void run_continuous_test(i2c_t *i2c)
{
    printf("=== Mode: Continuous Classification ===\n\r\n\r");

    classifier_init();

    imu_sample_t window_copy[CLF_WINDOW_SIZE];
    int idx = 0;
    int window_count = 0;
    long sample_count = 0;

    while (NUM_WINDOWS == 0 || window_count < NUM_WINDOWS) {
        imu_sample_t s;
        if (read_sample(i2c, &s) != 0) {
            printf("ERROR: gagal baca sampel dari sensor!\n\r");
            return;
        }

        if (idx < CLF_WINDOW_SIZE) {
            window_copy[idx++] = s;
        }

        if (VERBOSE_RAW) {
            printf("  [%ld] ax=%6d ay=%6d az=%6d gx=%6d gy=%6d gz=%6d\n\r",
                   sample_count, s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
        }

        activity_t result = classifier_update(&s);
        sample_count++;

        if (result != ACTIVITY_UNKNOWN) {
            window_count++;
            printf("[Window %d] Klasifikasi: %s\n\r",
                   window_count, activity_name(result));

            if (SHOW_FEATURES) {
                print_features(window_copy, idx);
            }

            idx = 0;
        }

        if (!VERBOSE_RAW && (sample_count % 50 == 0)) {
            printf("  ... %d window, %ld sampel, hasil terakhir: %s\n\r",
                   window_count, sample_count, activity_name(result));
        }

        simple_delay(SAMPLE_DELAY_LOOPS);
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("==============================================\n\r");
    printf("  Activity Classifier: Hardware Test\n\r");
    printf("  Platform: PULPissimo + MPU-6050\n\r");
    printf("==============================================\n\r\n\r");

    i2c_t *i2c = mpu6050_open();
    if (i2c == NULL) {
        printf("Test dibatalkan: i2c_open gagal.\n\r");
        return -1;
    }
    printf("[OK] i2c_open succeeded\n\r");

    int ret = mpu6050_init(i2c);
    if (ret != MPU6050_OK) {
        printf("Test dibatalkan: mpu6050_init gagal (%s)\n\r",
               mpu6050_err_name(ret));
        i2c_close(i2c);
        return ret;
    }

    run_continuous_test(i2c);

    /* run_continuous_test() hanya berhenti jika NUM_WINDOWS != 0 atau
     * terjadi error baca sensor. Dengan NUM_WINDOWS = 0, baris di bawah
     * ini praktis tidak pernah tercapai selama sensor tetap terbaca. */
    printf("\n\r==============================================\n\r");
    printf("  Test hardware berhenti (lihat pesan error di atas).\n\r");
    printf("==============================================\n\r");

    i2c_close(i2c);
    return 0;
}
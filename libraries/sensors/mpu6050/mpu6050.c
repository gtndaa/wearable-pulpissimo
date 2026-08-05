#include <stdint.h>
#include <stdio.h>
#include "pulp.h"
#include "mpu6050.h"

static int mpu6050_write_reg(i2c_t *i2c, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_write(i2c, buf, 2, 1);
}

static int mpu6050_read_reg(i2c_t *i2c, uint8_t reg, uint8_t *out, int len)
{
    int ret = i2c_write(i2c, &reg, 1, 0);
    if (ret != 0) return ret;
    int bytes = i2c_read(i2c, out, len, 0);
    return (bytes == len) ? 0 : -1;
}

i2c_t *mpu6050_open(void)
{
    i2c_dev_t dev;
    i2c_dev_init(&dev);
    dev.id           = 0;
    dev.cs           = MPU6050_ADDR << 1;
    dev.max_baudrate = 400000;

    i2c_t *i2c = i2c_open(&dev);
    if (i2c == NULL) {
        printf("[ERROR] i2c_open failed\n\r");
        return NULL;
    }
    i2c_settimeout(10000, true);
    return i2c;
}

int mpu6050_init(i2c_t *i2c)
{
    uint8_t who_am_i = 0;

    if (mpu6050_write_reg(i2c, REG_PWR_MGMT_1, PWR_MGMT_1_VAL) != 0) {
        printf("[ERROR] failed to wake up MPU6050\n\r");
        return MPU6050_ERR_CFG;
    }

    // stabilize after wakeup (datasheet recommends 100ms)
    for (volatile int d = 0; d < 50000; d++);

    if (mpu6050_read_reg(i2c, REG_WHO_AM_I, &who_am_i, 1) != 0) {
        printf("[ERROR] failed i2c comm on WHO_AM_I\n\r");
        return MPU6050_ERR_COMM;
    }
    if (who_am_i != WHO_AM_I_VALUE) {
        printf("[ERROR] wrong WHO_AM_I (got 0x%02X, expected 0x%02X)\n\r",
               who_am_i, WHO_AM_I_VALUE);
        return MPU6050_ERR_WHO_AM_I;
    }
    printf("[OK] WHO_AM_I OK (0x%02X)\n\r", who_am_i);

    if (mpu6050_write_reg(i2c, REG_SMPLRT_DIV, SMPLRT_DIV_VAL) != 0) {
        printf("[ERROR] failed to write SMPLRT_DIV\n\r");
        return MPU6050_ERR_CFG;
    }
    if (mpu6050_write_reg(i2c, REG_CONFIG, CONFIG_VAL) != 0) {
        printf("[ERROR] failed to write CONFIG\n\r");
        return MPU6050_ERR_CFG;
    }
    if (mpu6050_write_reg(i2c, REG_GYRO_CONFIG, GYRO_CONFIG_VAL) != 0) {
        printf("[ERROR] failed to write GYRO_CONFIG\n\r");
        return MPU6050_ERR_CFG;
    }
    if (mpu6050_write_reg(i2c, REG_ACCEL_CONFIG, ACCEL_CONFIG_VAL) != 0) {
        printf("[ERROR] failed to write ACCEL_CONFIG\n\r");
        return MPU6050_ERR_CFG;
    }

    printf("[OK] Init succeeded\n\r");
    return MPU6050_OK;
}

int mpu6050_read_accel(i2c_t *i2c, accel_data_t *out)
{
    uint8_t raw[6] = {0};

    if (mpu6050_read_reg(i2c, REG_ACCEL_XOUT_H, raw, 6) != 0) {
        printf("[ERROR] accel burst read failed\n\r");
        return MPU6050_ERR_READ;
    }

    // MPU6050 big-endian: high byte first
    int16_t x_raw = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t y_raw = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t z_raw = (int16_t)((raw[4] << 8) | raw[5]);

    // Convert to milli-g: mg = raw * 1000 / 16384
    out->x = (int32_t)x_raw * 1000 / ACCEL_SENS_2G;
    out->y = (int32_t)y_raw * 1000 / ACCEL_SENS_2G;
    out->z = (int32_t)z_raw * 1000 / ACCEL_SENS_2G;

    return MPU6050_OK;
}

int mpu6050_read_gyro(i2c_t *i2c, gyro_data_t *out)
{
    uint8_t raw[6] = {0};

    if (mpu6050_read_reg(i2c, REG_GYRO_XOUT_H, raw, 6) != 0) {
        printf("[ERROR] gyro burst read failed\n\r");
        return MPU6050_ERR_READ;
    }

    int16_t x_raw = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t y_raw = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t z_raw = (int16_t)((raw[4] << 8) | raw[5]);

    // Convert to milli-dps: mdps = raw * 1000 / 131
    out->x = (int32_t)x_raw * 1000 / GYRO_SENS_250DPS;
    out->y = (int32_t)y_raw * 1000 / GYRO_SENS_250DPS;
    out->z = (int32_t)z_raw * 1000 / GYRO_SENS_250DPS;

    return MPU6050_OK;
}

int mpu6050_read_all(i2c_t *i2c, accel_data_t *accel, gyro_data_t *gyro)
{
    uint8_t raw[14] = {0};

    // Burst read 14 bytes: ACCEL(6) + TEMP(2) + GYRO(6)
    if (mpu6050_read_reg(i2c, REG_ACCEL_XOUT_H, raw, 14) != 0) {
        printf("[ERROR] combined burst read failed\n\r");
        return MPU6050_ERR_READ;
    }

    int16_t ax = (int16_t)((raw[0]  << 8) | raw[1]);
    int16_t ay = (int16_t)((raw[2]  << 8) | raw[3]);
    int16_t az = (int16_t)((raw[4]  << 8) | raw[5]);
    // raw[6],raw[7] = TEMP, skip
    int16_t gx = (int16_t)((raw[8]  << 8) | raw[9]);
    int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    accel->x = (int32_t)ax * 1000 / ACCEL_SENS_2G;
    accel->y = (int32_t)ay * 1000 / ACCEL_SENS_2G;
    accel->z = (int32_t)az * 1000 / ACCEL_SENS_2G;

    gyro->x = (int32_t)gx * 1000 / GYRO_SENS_250DPS;
    gyro->y = (int32_t)gy * 1000 / GYRO_SENS_250DPS;
    gyro->z = (int32_t)gz * 1000 / GYRO_SENS_250DPS;

    return MPU6050_OK;
}
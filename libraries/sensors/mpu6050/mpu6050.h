#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include "pulp.h"

// AD0 → VCC : 7-bit=0x69 → 8-bit=0xD2
// AD0 → GND : 7-bit=0x68 → 8-bit=0xD0
#define MPU6050_ADDR     0x69   // AD0 → VCC

#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_INT_ENABLE   0x38
#define REG_ACCEL_XOUT_H 0x3B
#define REG_GYRO_XOUT_H  0x43
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

#define WHO_AM_I_VALUE   0x68   // fixed value regardless of AD0

// reg values for init
#define PWR_MGMT_1_VAL   0x01  // clear SLEEP, use PLL X gyro ref
#define SMPLRT_DIV_VAL   0x07  // 1kHz / (1+7) = 125Hz
#define CONFIG_VAL       0x03  // DLPF 44Hz accel / 42Hz gyro
#define GYRO_CONFIG_VAL  0x00  // ±250 dps
#define ACCEL_CONFIG_VAL 0x00  // ±2g

// sensitivity values for converting raw readings to physical units
#define ACCEL_SENS_2G      16384   // LSB/g  → mg  = raw * 1000 / 16384
#define GYRO_SENS_250DPS   131     // LSB/dps → mdps = raw * 1000 / 131

#define MPU6050_OK           0
#define MPU6050_ERR_I2C_OPEN -1
#define MPU6050_ERR_COMM     -2
#define MPU6050_ERR_WHO_AM_I -3
#define MPU6050_ERR_CFG      -4
#define MPU6050_ERR_READ     -5

typedef struct {
    int32_t x;   // milli-g
    int32_t y;
    int32_t z;
} accel_data_t;

typedef struct {
    int32_t x;   // milli-dps
    int32_t y;
    int32_t z;
} gyro_data_t;

i2c_t *mpu6050_open(void);
int    mpu6050_init(i2c_t *i2c);
int    mpu6050_read_accel(i2c_t *i2c, accel_data_t *out);
int    mpu6050_read_gyro(i2c_t *i2c, gyro_data_t *out);
int    mpu6050_read_all(i2c_t *i2c, accel_data_t *accel, gyro_data_t *gyro);

#endif
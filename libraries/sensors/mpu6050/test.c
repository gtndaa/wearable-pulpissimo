#include <stdint.h>
#include <stdio.h>
#include "pulp.h"
#include "mpu6050.h"

void pe_start(void) {}

int main(void)
{
    printf("[OK] MPU6050 initialize\n\r");

    i2c_t *i2c = mpu6050_open();
    if (i2c == NULL) return -1;
    printf("[OK] i2c_open succeeded\n\r");

    int ret = mpu6050_init(i2c);
    if (ret != MPU6050_OK) {
        printf("[ERROR] mpu6050_init failed, code=%d\n\r", ret);
        i2c_close(i2c);
        return ret;
    }
    printf("[OK] mpu6050_init succeeded\n\r");

    accel_data_t accel;
    gyro_data_t  gyro;

    while (1) {
        ret = mpu6050_read_all(i2c, &accel, &gyro);
        if (ret == MPU6050_OK) {
            printf("ACC X=%dmg Y=%dmg Z=%dmg | GYRO X=%dmdps Y=%dmdps Z=%dmdps\n\r",
                   (int)accel.x, (int)accel.y, (int)accel.z,
                   (int)gyro.x,  (int)gyro.y,  (int)gyro.z);
        } else {
            printf("[ERROR] read_all failed, code=%d\n\r", ret);
        }

        for (volatile int d = 0; d < 500000; d++);
    }

    i2c_close(i2c);
    return 0;
}